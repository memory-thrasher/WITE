#pragma once

#include <atomic>

namespace WITE {

  class syncLock {
  public:
    syncLock();
    void WaitForLock(bool busy = false);
    void ReleaseLock();
    void yield();
    bool isHeld();
  private:
    typedef std::atomic<uint64_t> ticket_t;
    ticket_t queueSeed, queueCurrent;
    uint32_t owningThread;
  };

  class scopeLock {
  public:
    void yield();
    void release();
    scopeLock(syncLock* lock);
    scopeLock(scopeLock& o);
    ~scopeLock();
  private:
    syncLock *lock;
    bool held;
  };

}
