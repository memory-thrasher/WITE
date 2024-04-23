#include <stdlib.h>

#include "syncLock.hpp"
#include "thread.hpp"
#include "profiler.hpp"

namespace WITE {

  syncLock::syncLock() {}

  void syncLock::WaitForLock(bool busy) {
    uint64_t seed;
    seed = queueSeed.fetch_add(1);//take a number
    uint32_t sleepCnt = 0;
    while (seed > queueCurrent.load())
      if(!busy) [[likely]]
	thread::sleepShort(sleepCnt);
  }

  void syncLock::ReleaseLock() {
    queueCurrent.fetch_add(1);
  }

  void syncLock::yield() {
    uint64_t newSeed;
    newSeed = queueSeed.fetch_add(1);
    queueCurrent.fetch_add(1);
    uint32_t sleepCnt = 0;
    while (newSeed > queueCurrent.load()) thread::sleepShort(sleepCnt);
  }

  bool syncLock::isHeld() {
    return queueSeed.load(std::memory_order_consume) > queueCurrent.load(std::memory_order_consume);
  };

  scopeLock::scopeLock(syncLock * sl) {
    sl->WaitForLock(false);//maybe make this optional?
    lock = sl;
    held = true;
  }

  scopeLock::scopeLock(scopeLock&& o) : lock(o.lock), held(o.held) {
    o.lock = NULL;
    o.held = false;
  }

  void scopeLock::release() {
    if(held)
      lock->ReleaseLock();
    held = false;
  };

  scopeLock::~scopeLock() {
    if (held)
      lock->ReleaseLock();
    held = false;
  }

  void scopeLock::yield() {
    lock->yield();
  }

};
