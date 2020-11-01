#include "Internal.h"

#define HOLDER_NONE (std::numeric_limits<decltype(SyncLock::holder)>::max())

namespace WITE {

  /*uint64_t timeNs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((((LONGLONG)ft.dwHighDateTime) << 32LL) + (LONGLONG)ft.dwLowDateTime) * 100;
    //wraparound is ~June 2185;  64 bit uint in nanoseconds
    }*/

  //SyncLock::mutex_t SyncLock::seed(0);

	SyncLock::SyncLock() {}
	//: id(seed.fetch_add(1, std::memory_order_relaxed)) {}

  SyncLock::~SyncLock() {}

  void SyncLock::WaitForLock() {
    //uint32_t tid = Thread::getCurrentTid();
    size_t seed;
    /*if (holder == tid) {
      holds++;
      return;
    }*/
    seed = queueSeed.fetch_add(1, std::memory_order_acq_rel);//take a number
    while (seed > queueCurrent.load(std::memory_order_consume)) sleep(1);
    //holder = tid;
  }

  void SyncLock::ReleaseLock() {
    /*if (holder != Thread::getCurrentTid()) CRASH("Failed to free synclock\n");
    holds--;
    if (!holds) {
      holder = HOLDER_NONE;*/
      queueCurrent.fetch_add(1, std::memory_order_release);
    //}
  }

  void SyncLock::yield() {
    uint64_t preHolds, newSeed;
    /*uint32_t tid = Thread::getCurrentTid();
    preHolds = holds;
    holds = 0;
    holder = HOLDER_NONE;*/
    newSeed = queueSeed.fetch_add(1, std::memory_order_acq_rel);
    queueCurrent.fetch_add(1, std::memory_order_release);
    while (newSeed > queueCurrent.load(std::memory_order_consume)) sleep(1);
    /*holder = tid;
    holds = preHolds;*/
  }

  /*uint64_t SyncLock::getId() {
    return id;
  }

  bool SyncLock::isLocked() {
    return holder == HOLDER_NONE;
  }*/

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
