#include <stdlib.h>

#include "SyncLock.hpp"
#include "Thread.hpp"

namespace WITE::Util {

  SyncLock::SyncLock() {}

  void SyncLock::WaitForLock(bool busy) {
    uint64_t seed;
    seed = queueSeed.fetch_add(1, std::memory_order_acq_rel);//take a number
    while (seed > queueCurrent.load(std::memory_order_consume))
      if(!busy)
	Platform::Thread::sleepShort();
    owningThread = Platform::Thread::getCurrentTid();
  }

  void SyncLock::ReleaseLock() {
    owningThread = ~0;
    queueCurrent.fetch_add(1, std::memory_order_release);
  }

  void SyncLock::yield() {
    uint64_t newSeed;
    newSeed = queueSeed.fetch_add(1, std::memory_order_acq_rel);
    owningThread = ~0;
    queueCurrent.fetch_add(1, std::memory_order_release);
    while (newSeed > queueCurrent.load(std::memory_order_consume)) Platform::Thread::sleepShort();
    owningThread = Platform::Thread::getCurrentTid();
  }

  ScopeLock::ScopeLock(SyncLock * sl) {
    sl->WaitForLock(false);//maybe make this optional?
    lock = sl;
  }

  ScopeLock::ScopeLock(ScopeLock& o) : lock(o.lock) {
    o.lock = NULL;
  }

  ScopeLock::~ScopeLock() {
    if (lock) lock->ReleaseLock();
    lock = NULL;
  }

  void ScopeLock::yield() {
    lock->yield();
  }

};
