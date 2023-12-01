#include "time.hpp"
#include "advancedSyncLock.hpp"
#include "Thread.hpp"
#include "DEBUG.hpp"

namespace WITE {

  advancedSyncLock::advancedSyncLock() {};

  advancedSyncLock::~advancedSyncLock() {
    scopeLock lock(&mutex);
    currentOwner = ~0;
    holds = (~0)>>1;
  };

  bool advancedSyncLock::acquire(uint64_t timeoutNS) {
    timespec startTime = Util::now();
    auto tid = Platform::Thread::getCurrentTid();
    do {
      scopeLock lock(&mutex);
      if(holds == 0 || currentOwner == tid) {
	currentOwner = tid;
	holds++;
	return true;
      }
      lock.release();
      if(timeoutNS > 0)
	Platform::Thread::sleepShort();
    } while(Util::toNS(Util::since(startTime)) < timeoutNS);
    return false;
  };

  void advancedSyncLock::release() {
    scopeLock lock(&mutex);
    auto tid = Platform::Thread::getCurrentTid();
    ASSERT_TRAP(holds > 0, "Mutex hold underflow!");
    ASSERT_TRAP(currentOwner == tid, "Mutex Failure!!! tid: ", tid, " owner: ", currentOwner, " holds: ", holds);
    --holds;
  };

  bool advancedSyncLock::heldBy(uint32_t tid) {
    scopeLock lock(&mutex);
    return holds && currentOwner == tid;
  };

  bool advancedSyncLock::isHeld() {
    scopeLock lock(&mutex);
    return holds;
  };

  advancedScopeLock::advancedScopeLock(advancedSyncLock& l, uint64_t timeoutNS) : l(l) {
    reacquire(timeoutNS);
  };

  advancedScopeLock::~advancedScopeLock() {
    if(isHeld())
      release();
  };

  void advancedScopeLock::reacquire(uint64_t timeoutNS) {
    l.acquire(timeoutNS);
  };

  void advancedScopeLock::release() {
    l.release();
  };

};
