#include "SyncLock.hpp"

namespace WITE::Util {

  SyncLock::SyncLock() {}

  void SyncLock::WaitForLock(bool busy) {
    size_t seed;
    seed = queueSeed.fetch_add(1, std::memory_order_acq_rel);//take a number
    while (seed > queueCurrent.load(std::memory_order_consume))
      if(!busy)
	std::this_thread::sleep_for(std::chrono::micorseconds(10));
  }

  void SyncLock::ReleaseLock() {
    queueCurrent.fetch_add(1, std::memory_order_release);
  }

  void SyncLock::yield() {
    uint64_t newSeed;
    newSeed = queueSeed.fetch_add(1, std::memory_order_acq_rel);
    queueCurrent.fetch_add(1, std::memory_order_release);
    while (newSeed > queueCurrent.load(std::memory_order_consume)) sleep(1);
  }

  ScopeLock::ScopeLock(SyncLock * sl) {
    sl->WaitForLock();
    lock = sl;
  }

  ScopeLock::~ScopeLock() {
    if (lock) lock->ReleaseLock();
    lock = NULL;
  }

  void ScopeLock::yield() {
    lock->yield();
  }

};
