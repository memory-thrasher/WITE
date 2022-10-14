#include <time.h>
#include <limits>
#include <algorithm>
#include <signal.h>

#include "DBThread.hpp"
#include "Thread.hpp"
#include "DEBUG.hpp"
#include "Database.hpp"
#include "StdExtensions.hpp"

#define MAX_DELTA_AGE 5

namespace WITE::DB {

  DBThread::DBThread(Database* const db, size_t tid) :
    dbId(tid),
    db(db),
    slice_withUpdates(),
    slice_withoutUpdates(),
    slice_toBeRemoved(),
    slice_toBeAdded(),
    transactionPool() {}

  bool DBThread::setState(const semaphoreState old, const semaphoreState desired) {
    ASSERT_TRAP(desired <= state_exited, "illegal semaphore state");
    semaphoreState state = old;
    return semaphore.compare_exchange_strong(state, desired, std::memory_order_relaxed, std::memory_order_relaxed);
  }

  bool DBThread::waitForState(semaphoreState desired) {
    while(true) {
      semaphoreState state = semaphore.load(std::memory_order_consume);
      if(state == desired)
	return true;
      if(state == state_exited || state == state_exiting || state == state_exploded)
	return false;
      db->idle(this);
    }
  }

  bool DBThread::waitForState(const semaphoreState old, const semaphoreState desired) {
    semaphoreState state = semaphore.load(std::memory_order_consume);
    while(state != desired) {
      if(state != old)
	return false;
      db->idle(this);
      state = semaphore.load(std::memory_order_consume);
    }
    return true;
  }

  void DBThread::workLoop() {
    this->thread = PThread::current();
    static const struct itimerspec FRAME_MAX_TIME { { 0, 0 }, { std::numeric_limits<time_t>::max(), 0 } };
    struct itimerspec frameTime;
    struct sigevent noop;
    noop.sigev_notify = SIGEV_NONE;
    timer_t cpuTimer;
    if(timer_create(CLOCK_THREAD_CPUTIME_ID, &noop, &cpuTimer)) {
      ERROR("FAILED to start db thread because timing is unavailable");
      return;
    }
    setState(state_initial, state_ready);
    waitForState(state_ready, state_updating);
    try {
      while(1) {
	if(timer_settime(cpuTimer, 0, &FRAME_MAX_TIME, NULL))
	  WARN("Timer set fail, thread balancing may be wrong");
	if(semaphore != state_updating)
	  break;
	DBRecord data;
	for(DBEntity* ent : slice_withUpdates) {
	  ent->read(&data);
	  ASSERT_TRAP(data.header.flags >> DBRecordFlag::allocated, "unallocated entity in slice, frame: ", db->getFrame(), ", ent: ", ent->getId());
	  auto type = db->getType(data.header.type);
	  ASSERT_TRAP(type->onUpdate, "onUpdate missing");
	  type->onUpdate(&data, ent);
	}
	if(timer_gettime(cpuTimer, &frameTime))
	  WARN("Timer get fail, thread balancing may be wrong");
	nsSpentOnLastFrame = (FRAME_MAX_TIME.it_value.tv_sec - frameTime.it_value.tv_sec) * 1000000000 +
	  (FRAME_MAX_TIME.it_value.tv_nsec - frameTime.it_value.tv_nsec);
	//state sync so changes to this thread's slice don't block other threads adding rows to this thread.
	if(!setState(state_updating, state_updated) || !waitForState(state_updated, state_maintaining))
	  break;
	//shifts in slices should not count against this threads work units because they are unrelated to slice size
	if(slice_toBeAdded.size() + slice_toBeRemoved.size()) {
	  Util::ScopeLock lock(&sliceAlterationPoolMutex);
	  if(slice_toBeRemoved.size()) {
	    auto up = [this](auto e){ return Collections::contains(slice_toBeRemoved, e); };
	    Collections::remove_if(slice_withUpdates, up);
	    Collections::remove_if(slice_withoutUpdates, up);
	    temp_uniqTypes.clear();
	    Collections::uniq(slice_toBeRemoved, [](DBEntity* dbe) { return dbe->getType(); }, temp_uniqTypes);
	    for(int i = 0;i < temp_uniqTypes.size();i++)
	      typeIndex[temp_uniqTypes[i]].remove_if(up);
#ifdef DEBUG_THREAD_SLICES
	    for(DBEntity* ent : slice_toBeRemoved) {
	      ent->lastSliceRemovedFrame = db->getFrame();
	      ent->lastSliceRemovedOpIdx = ent->operationIdx++;
	      ent->lastMasterThread = ent->masterThread;
	    }
#endif
	    slice_toBeRemoved.clear();
	  }
	  if(slice_toBeAdded.size()) {
	    DBDelta allocationEvent;
	    allocationEvent.clear();
	    allocationEvent.flagWriteValues = DBRecordFlag::allocated;
	    allocationEvent.write_type = true;
	    // allocationEvent.write_nextGlobalId//TODO head flag, any nextIds, put how many need to be allocated in type?
	    allocationEvent.flagWriteMask = DBRecordFlag::all;
	    for(auto i = slice_toBeAdded.begin();i != slice_toBeAdded.end();i++) {
#ifdef DEBUG_THREAD_SLICES
	      i->first->lastSliceAddedFrame = db->getFrame();
	      i->first->lastSliceAddedOpIdx = i->first->operationIdx++;
#endif
	      auto type = db->getType(i->second);
	      (type->onUpdate ? slice_withUpdates : slice_withoutUpdates).push_back(i->first);
	      typeIndex[type->typeId].append(i->first);
	      allocationEvent.new_type = i->second;
	      i->first->write(&allocationEvent);
	      if(type->onAllocate)
		type->onAllocate(i->first);
	      if(type->onSpinUp)
		type->onSpinUp(i->first);
	    }
	    slice_toBeAdded.clear();
	  }
	}
	if(db->getFrame() > MAX_DELTA_AGE) {
	  size_t oldFrame = db->getFrame() - MAX_DELTA_AGE;
	  auto tx = transactionPool.peek();
	  while(tx && tx->frame <= oldFrame) {
	    db->idle(this);
	    tx = transactionPool.peek();
	  }
	}
	if(!setState(state_maintaining, state_maintained) || !waitForState(state_maintained, state_updating))
	  break;
      }
      timer_delete(cpuTimer);
      semaphore = state_exited;
    } catch(std::exception ex) {
      timer_delete(cpuTimer);
      semaphore = state_exploded;
    }
  }

  void DBThread::start() {
    if(semaphore != state_initial)
      return;
    PThread::spawnThread(PThread::threadEntry_t_F::make(this, &DBThread::workLoop));
    waitForState(state_initial, state_ready);
  }

  void DBThread::addToSlice(DBEntity* in, DBRecord::type_t type) {
    Util::ScopeLock lock(&sliceAlterationPoolMutex);
    in->masterThread = dbId;
    slice_toBeAdded.push_back(std::pair(in, type));
  }

  void DBThread::removeFromSlice(DBEntity* in) {
    Util::ScopeLock lock(&sliceAlterationPoolMutex);
    slice_toBeRemoved.push_back(in);
  }

  void DBThread::join() {
    if(semaphore == state_exited) return;
    semaphore = state_exiting;
    semaphoreState state = semaphore.load(std::memory_order_consume);
    while(state != state_exited) {
      ASSERT_TRAP(state == state_exiting, "semaphore left exiting state");
      state = semaphore.load(std::memory_order_consume);
    }
  }

  uint32_t DBThread::getCurrentTid() {
    return PThread::getCurrentTid();
  }

}
