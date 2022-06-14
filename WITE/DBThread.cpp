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

  DBThread::DBThread(Database* const db, size_t tid) : dbId(tid), db(db), slice(10000), slice_toBeRemoved(1000),
							slice_toBeAdded(1000), transactions() {}

  bool DBThread::setState(semaphoreState desired) {
    semaphoreState state;
    do {
      state = semaphore.load(std::memory_order_acquire);
      if(state == state_exiting || state == state_exploded)
	return false;
    } while(!semaphore.compare_exchange_strong(state, desired));
    return true;
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
    try {
      while(1) {
	if(timer_settime(cpuTimer, 0, &FRAME_MAX_TIME, NULL))
	  WARN("Timer set fail, thread balancing may be wrong");
	if(!setState(state_updating)) break;
	DBRecord data;
	for(DBEntity* ent : slice) {
	  ent->read(&data);
	  db->getType(data.header.type)->update(&data, ent);
	}
	if(!setState(state_updated) || !waitForState(state_maintaining)) break;
	if(timer_gettime(cpuTimer, &frameTime))
	  WARN("Timer get fail, thread balancing may be wrong");
	nsSpentOnLastFrame = (FRAME_MAX_TIME.it_value.tv_sec - frameTime.it_value.tv_sec) * 1000000000 +
	  (FRAME_MAX_TIME.it_value.tv_nsec - frameTime.it_value.tv_nsec);
	//shifts in slices should not count against this threads work units because they are unrelated to slice size
	if(slice_toBeAdded.size() + slice_toBeRemoved.size()) {
	  Util::ScopeLock lock(&sliceAlterationPoolMutex);
	  if(slice_toBeRemoved.size()) {
	    Collections::remove_if(slice, [this](auto e){ return Collections::contains(slice_toBeRemoved, e); });
	    slice_toBeRemoved.clear();
	  }
	  if(slice_toBeAdded.size())
	    Collections::concat_move(slice, slice_toBeAdded);
	  if(!setState(state_frameSync) || !waitForState(state_updating)) break;
	}
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
    waitForState(state_updating);
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
    setState(state_exiting);
    waitForState(state_exited);
  }

  uint32_t DBThread::getCurrentTid() {
    return PThread::getCurrentTid();
  }

  uint32_t DBThread::getTid() {
    return thread->getTid();
  }

}
