#pragma once

#include "SyncLock.hpp"

namespace WITE::Util {

  class AdvancedSyncLock {
  private:
    SyncLock mutex;
    uint32_t currentOwner, holds;
  public:
    AdvancedSyncLock();
    ~AdvancedSyncLock();
    bool acquire(uint64_t timeoutNS = ~0);//0 means now or never. Execution maytake much longer than specified timeout.
    void release();
    bool heldBy(uint32_t tid);
    bool isHeld();
    bool inline isHeldByThisThread() { return heldBy(Thread::getCurrentTid()); };
  };

  class AdvancedScopeLock {
  private:
    AdvancedSyncLock& l;
  public:
    AdvancedScopeLock(AdvancedSyncLock& l, uint64_t timeoutNS = ~0);
    ~AdvancedScopeLock();
    void release();
    void reacquire(uint64_t timeoutNS = ~0);
    inline bool isHeld() { return l.isHeldByThisThread(); };//must check if timeout specified!
  };

};

