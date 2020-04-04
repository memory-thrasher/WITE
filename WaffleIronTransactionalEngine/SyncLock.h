#pragma once

namespace WITE {

  class SyncLock {
  public:
    SyncLock();
    ~SyncLock();
    void WaitForLock();
    void ReleaseLock();
    uint64_t getId();
    bool isLocked();
    inline void yield();
  private:
    typedef std::atomic_uint64_t mutex_t;
    static mutex_t seed;
    uint64_t id, holds;
    mutex_t holder, queueSeed, queueCurrent;
  };

  class ScopeLock {
  public:
    inline void yield();
    ScopeLock(const SyncLock * lock);
    ~ScopeLock();
  private:
    const SyncLock *lock;
  };

}
