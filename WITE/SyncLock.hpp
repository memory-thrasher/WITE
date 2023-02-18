#pragma once

#include <atomic>

namespace WITE::Util {

  class SyncLock {
  public:
    SyncLock();
    void WaitForLock(bool busy = false);
    void ReleaseLock();
    void yield();
    bool isHeld();
  private:
    typedef std::atomic<uint64_t> ticket_t;
    ticket_t queueSeed, queueCurrent;
    uint32_t owningThread;
  };

  class ScopeLock {
  public:
    inline void yield();
    inline void release();
    ScopeLock(SyncLock* lock);
    ScopeLock(ScopeLock& o);
    ~ScopeLock();
  private:
    SyncLock *lock;
    bool held;
  };

}
