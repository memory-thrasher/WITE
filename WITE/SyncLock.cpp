#include <stdlib.h>

#include "SyncLock.hpp"
#include "Thread.hpp"

namespace WITE::Util {

  SyncLock::SyncLock() {}

  void SyncLock::WaitForLock(bool busy) {
    uint64_t seed;
    seed = queueSeed.fetch_add(1);//take a number
    while (seed > queueCurrent.load())
      if(!busy)
	Platform::Thread::sleepShort();
    owningThread = Platform::Thread::getCurrentTid();
  }

  void SyncLock::ReleaseLock() {
    owningThread = ~0;
    queueCurrent.fetch_add(1);
  }

  void SyncLock::yield() {
    uint64_t newSeed;
    newSeed = queueSeed.fetch_add(1);
    owningThread = ~0;
    queueCurrent.fetch_add(1);
    while (newSeed > queueCurrent.load()) Platform::Thread::sleepShort();
    owningThread = Platform::Thread::getCurrentTid();
  }

  bool SyncLock::isHeld() {
    return queueSeed.load(std::memory_order_consume) > queueCurrent.load(std::memory_order_consume);
  };

  ScopeLock::ScopeLock(SyncLock * sl) {
    sl->WaitForLock(false);//maybe make this optional?
    lock = sl;
    held = true;
  }

  ScopeLock::ScopeLock(ScopeLock& o) : lock(o.lock) {
    o.lock = NULL;
    held = false;
  }

  void ScopeLock::release() {
    if(held)
      lock->ReleaseLock();
    held = false;
  };

  ScopeLock::~ScopeLock() {
    if (held)
      lock->ReleaseLock();
    held = false;
  }

  void ScopeLock::yield() {
    lock->yield();
  }

};
