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
    uint32_t holder;
    mutex_t queueSeed, queueCurrent;
  };

  class ScopeLock {
  public:
    inline void yield();
    ScopeLock(SyncLock * lock);
    ~ScopeLock();
  private:
    SyncLock *lock;
  };

}
