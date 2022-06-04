#pragma once

#include <atomic>

namespace WITE::Util {

  class SyncLock {
  public:
    SyncLock();
    void WaitForLock(bool busy);
    void ReleaseLock();
    void yield();
  private:
    typedef std::atomic<uint64_t> ticket_t;
    ticket_t queueSeed, queueCurrent;
  };

  class ScopeLock {
  public:
    inline void yield();
    ScopeLock(SyncLock* lock);
    ~ScopeLock();
  private:
    SyncLock *lock;
  };

}
