#include <timme.h>
#include <limits>
#include <algorithm>

#include "DBThread.hpp"

namespace WITE::DB {

  DBThread::DBThread(Database const * db, size_t tid) : dbI(tid), db(db), slice(10000), slice_toBeRemoved(1000),
							slice_toBeAdded(1000), transactions() {}

  void DBThread::workLoop() {
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
	semaphore = state_updating;
	DBRecord data;
	for(DBEntity* ent : slice) {
	  ent->read(&data);
	  db->getType(data.type)->update.call(&data);
	}
	auto state = state_updating;
	semaphore.compare_exchange_strong(&state, state_updated);
	while((state = semaphore) != state_maintaining && state != state_exiting);
	if(state == state_exiting) break;
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
	slice.splice(slice.end(), slice_toBeAdded);//splice moves, so this clears toBeAdded
      }
      time_delete(cpuTimer);
    } catch {
      time_delete(cpuTimer);
      semaphore = state_exploded;
    }
  }

}
