#include "time.h"
#include "AdvancedSyncLock.hpp"
#include "Thread.hpp"

namespace WITE::Util {

  AdvancedSyncLock::AdvancedSyncLock() {};

  AdvancedSyncLock::~AdvancedSyncLock() {
    ScopeLock lock(&mutex);
    currentOwner = ~0;
    holds = (~0)>>1;
  };

  bool AdvancedSyncLock::acquire(uint64_t timeoutNS) {
    timespec startTime = now();
    auto tid = Thread::getCurrentTid();
    do {
      ScopeLock lock(&mutex);
      if(holds == 0 || currentOwner == tid) {
	currentOwner = tid;
	holds++;
	return true;
      }
      lock.release();
      if(timeoutNS > 0)
	Thread::sleepShort();
    } while(since(startTime) < timeoutNS);
    return false;
  };

  bool AdvancedSyncLock::release() {
    ScopeLock lock(&mutex);
    ASSERT_TRAP(currentOwner == Thread::getCurrentTid(), "Mutex Failure!!!");
    --holds;
  };

  bool heldBy(uint32_t tid) {
    ScopeLock lock(&mutex);
    return holds && currentOwner == tid;
  };

  bool isHeld() {
    ScopeLock lock(&mutex);
    return holds;
  };

  AdvancedScopeLock::AdvancedScopeLock(AdvancedSyncLock& l, timeoutNS) : l(l) {
    reacquire(timeoutNS);
  };

  AdvancedScopeLock::~AdvancedScopeLock() {
    release();
  };

  AdvancedScopeLock::reacquire(uint64_t timeoutNS) {
    l.acquire(timeoutNS);
  };

  AdvancedScopeLock::release() {
    l.release();
  };

};
