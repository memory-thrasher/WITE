#include <time.h>
#include <limits>
#include <algorithm>
#include <signal.h>

#include "DBThread.hpp"

#include "Thread.hpp"
#include "DEBUG.hpp"
#include "Database.hpp"
#include "StdExtensions.hpp"

namespace WITE::DB {

  DBThread::DBThread(Database* const db, size_t tid) : dbId(tid), db(db), slice(), slice_toBeRemoved(),
						       slice_toBeAdded(), transactions() {}

  bool DBThread::setState(const semaphoreState old, const semaphoreState desired) {
    semaphoreState state = old;
    return semaphore.compare_exchange_strong(state, desired, std::memory_order_acq_rel, std::memory_order_consume);
  }

  bool DBThread::waitForState(semaphoreState desired) {
    while(true) {
      semaphoreState state = semaphore.load(std::memory_order_consume);
      if(state == desired)
	return true;
      if(state == state_exited || state == state_exiting || state == state_exploded)
	return false;
      PThread::sleep();
    }
  }

  bool DBThread::waitForState(const semaphoreState old, const semaphoreState desired) {
    semaphoreState state = semaphore.load(std::memory_order_consume);
    while(state != desired) {
      if(state != old)
	return false;
      PThread::sleep();
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
	if(semaphore != state_updating) break;
	DBRecord data;
	for(DBEntity* ent : slice) {
	  ent->read(&data);
	  db->getType(data.header.type)->update(&data, ent);
	}
	if(timer_gettime(cpuTimer, &frameTime))
	  WARN("Timer get fail, thread balancing may be wrong");
	db->flushTransactions(&transactions);
	nsSpentOnLastFrame = (FRAME_MAX_TIME.it_value.tv_sec - frameTime.it_value.tv_sec) * 1000000000 +
	  (FRAME_MAX_TIME.it_value.tv_nsec - frameTime.it_value.tv_nsec);
	//state sync so changes to this thread's slice don't block other threads adding rows to this thread.
	if(!setState(state_updating, state_updated) || !waitForState(state_updated, state_maintaining)) break;
	//shifts in slices should not count against this threads work units because they are unrelated to slice size
	if(slice_toBeAdded.size() + slice_toBeRemoved.size()) {
	  Util::ScopeLock lock(&sliceAlterationPoolMutex);
	  if(slice_toBeRemoved.size()) {
	    auto up = [this](auto e){ return Collections::contains(slice_toBeRemoved, e) };
	    Collections::remove_if(slice_withUpdates, up);
	    Collections::remove_if(slice_withoutUpdates, up);
	    temp_uniqTypes.clear();
	    Collections::uniq(slice_toBeRemoved, [](DBEntity* dbe) { return dbe->getType(); }, &temp_uniqTypes);
	    for(int i = 0;i < temp_uniqTypes.size();i++)
	      typeIndex[uniqTypes[i]].remove_if(up);
	    slice_toBeRemoved.clear();
	  }
	  if(slice_toBeAdded.size()) {
	    for(auto i = slice_toBeAdded.begin();i != slice_toBeAdded.end();i++) {
	      auto type = db->getType(i->getType());
	      (type.update ? slice_withUpdates : slice_withoutUpdates).push_back(*i);
	      typeIndex[type.typeId].push_back(*i);
	    }
	    slice_toBeAdded.clear();
	  }
	}
	if(!setState(state_maintaining, state_maintained) || !waitForState(state_maintained, state_updating)) break;
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

  void DBThread::addToSlice(DBEntity* in) {
    Util::ScopeLock lock(&sliceAlterationPoolMutex);
    slice_toBeAdded.push_back(in);
  }

  void DBThread::removeFromSlice(DBEntity* in) {
    Util::ScopeLock lock(&sliceAlterationPoolMutex);
    slice_toBeRemoved.push_back(in);
  }

  void DBThread::join() {
    semaphore = state_exiting;
    waitForState(state_exiting, state_exited);
  }

  uint32_t DBThread::getCurrentTid() {
    return PThread::getCurrentTid();
  }

  uint32_t DBThread::getTid() {
    return thread->getTid();
  }

}
