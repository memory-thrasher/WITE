#include <timme.h>
#include <limits>
#include <algorithm>

#include "DBThread.hpp"

namespace WITE::DB {

  DBThread::DBThread(Database const * db, size_t tid) : dbI(tid), db(db), slice(10000), slice_toBeRemoved(1000),
							slice_toBeAdded(1000), transactions() {}

  bool DBThread::setState(semaphoreState desired) {
    do {
      semaphoreState state = semaphore.load(std::memory_order_acquire);
      if(state == state_exiting || state == state_exploded)
	return false;
    } while(!semaphore.compare_exchange_strong(&state, desired));
  }

  bool DBThread::waitForState(semaphoreState desired) {
    while(true) {
      semaphoreState state = semaphore.load(std::memory_order_consume);
      if(state == desired)
	return true;
      if(state == state_exited || state_exiting || state == state_exploded)
	return false;
      Thread::sleep();
    }
  }

  void DBThread::workLoop() {
    this.thread = PThread::current();
    static const struct itimerspec FRAME_MAX_TIME { { 0, 0 }, { std::numeric_limits<time_t>::max, 0 } };
    struct itimerspec frameTime;
    struct sigevent noop;
    noop.sigev_notify = SIGEV_NONE;
    struct timer_t cpuTimer;
    if(timer_create(CLOCK_THREAD_CPUTIME_ID, &noop, &cpuTime)) {
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
	  db->getType(data.type)->update.call(&data);
	}
	if(!setState(state_updated) || !waitForState(state_maintaining)) break;
	if(timer_gettime(cpuTimer, &cpuTimer))
	  WARN("Timer get fail, thread balancing may be wrong");
	nsSpentOnLastFrame = (FRAME_MAX_TIME.it_value.tv_sec - cpuTime.it_value.tv_sec) * 1000000000 +
	  (FRAME_MAX_TIME.it_value.tv_nsec - cpuTime.it_value.tv_nsec);
	//shifts in slices should not count against this threads work units because they are unrelated to slice size
	if(slice_toBeRemoved.size()) {
	  slice.remove_if([](auto e){
			    return std::find(slice_toBeRemoved.begin(), slice_toBeRemoved.end(), e) != slice_toBeRemoved.end();
			  });
	  slice_toBeRemoved.clear();
	}
	{
	  ScopeLock lock(&sliceAlterationPoolMutex);
	  if(slice_toBeAdded.size())
	    slice.splice(slice.end(), slice_toBeAdded);//splice moves, so this clears toBeAdded
	  if(!setState(state_frameSync) || !waitForState(state_updating)) break;
	}
      }
      time_delete(cpuTimer);
      semaphore = state_exited;
    } catch {
      time_delete(cpuTimer);
      semaphore = state_exploded;
    }
  }

  void DBThread::start() {
    if(semaphore != state_initial)
      return;
    PThread.spawnThread(PThread::threadEntry_t_F::make(this, &DBThread::workLoop));
    waitForState(state_updating);
  }

  void DBThread::addToSlice(DBEntity* in) {
    ScopeLock lock(&sliceAlterationPoolMutex);
    slice_toBeAdded.push_back(in);
  }

  void DBThread::removeFromSlice(DBEntity* in) {
    ScopeLock lock(&sliceAlterationPoolMutex);
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
