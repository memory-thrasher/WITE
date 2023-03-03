#include "time.hpp"
#include "AdvancedSyncLock.hpp"
#include "Thread.hpp"
#include "DEBUG.hpp"

namespace WITE::Util {

  AdvancedSyncLock::AdvancedSyncLock() {};

  AdvancedSyncLock::~AdvancedSyncLock() {
    ScopeLock lock(&mutex);
    currentOwner = ~0;
    holds = (~0)>>1;
  };

  bool AdvancedSyncLock::acquire(uint64_t timeoutNS) {
    timespec startTime = now();
    auto tid = Platform::Thread::getCurrentTid();
    do {
      ScopeLock lock(&mutex);
      if(holds == 0 || currentOwner == tid) {
	currentOwner = tid;
	holds++;
	return true;
      }
      lock.release();
      if(timeoutNS > 0)
	Platform::Thread::sleepShort();
    } while(toNS(since(startTime)) < timeoutNS);
    return false;
  };

  void AdvancedSyncLock::release() {
    ScopeLock lock(&mutex);
    ASSERT_TRAP(currentOwner == Platform::Thread::getCurrentTid(), "Mutex Failure!!!");
    --holds;
  };

  bool AdvancedSyncLock::heldBy(uint32_t tid) {
    ScopeLock lock(&mutex);
    return holds && currentOwner == tid;
  };

  bool AdvancedSyncLock::isHeld() {
    ScopeLock lock(&mutex);
    return holds;
  };

  AdvancedScopeLock::AdvancedScopeLock(AdvancedSyncLock& l, uint64_t timeoutNS) : l(l) {
    reacquire(timeoutNS);
  };

  AdvancedScopeLock::~AdvancedScopeLock() {
    release();
  };

  void AdvancedScopeLock::reacquire(uint64_t timeoutNS) {
    l.acquire(timeoutNS);
  };

  void AdvancedScopeLock::release() {
    l.release();
  };

};
