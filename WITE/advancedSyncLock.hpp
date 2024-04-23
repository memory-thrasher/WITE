#pragma once

#include "thread.hpp"
#include "syncLock.hpp"

namespace WITE {

  class advancedSyncLock {
  private:
    syncLock mutex;
    tid_t currentOwner;
    uint32_t holds;
  public:
    advancedSyncLock();
    ~advancedSyncLock();
    bool acquire(uint64_t timeoutNS = ~0);//0 means now or never. Execution maytake much longer than specified timeout.
    void release();
    bool heldBy(tid_t tid);
    bool isHeld();
    bool inline isHeldByThisThread() { return heldBy(thread::getCurrentTid()); };
  };

  class advancedScopeLock {
  private:
    advancedSyncLock& l;
  public:
    advancedScopeLock(advancedSyncLock& l, uint64_t timeoutNS = ~0);
    ~advancedScopeLock();
    void release();
    void reacquire(uint64_t timeoutNS = ~0);
    inline bool isHeld() { return l.isHeldByThisThread(); };//must check if timeout specified!
  };

};

