#include "stdafx.h"
#include "SyncLock.h"
#include "Globals.h"
#include "Export.h"
#include "Thread.h"

namespace WITE {

  /*uint64_t timeNs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((((LONGLONG)ft.dwHighDateTime) << 32LL) + (LONGLONG)ft.dwLowDateTime) * 100;
    //wraparound is ~June 2185;  64 bit uint in nanoseconds
    }*/

  SyncLock::mutex_t SyncLock::seed = 0;

  SyncLock::SyncLock() : holds(0), id(seed.fetch_add(1, std::memory_order_relaxed)), holder(~0), queueSeed(0), queueCurrent(0) {}

  SyncLock::~SyncLock() {}

  void SyncLock::WaitForLock() {
    uint32_t tid = Thread::getCurrentTid();
    size_t seed;
    if (holder == tid) {
      holds++;
      return;
    }
    seed = queueSeed.fetch_add(1, std::memory_order_acq_rel);//take a number
    while (seed > queueCurrent.load(std::memory_order_consume)) sleep(1);
    holder = tid;
  }

  void SyncLock::ReleaseLock() {
    if (holder != Thread::getCurrentTid()) CRASH;
    holds--;
    if (!holds) {
      holder = ~0;
      queueCurrent.fetch_add(1, std::memory_order_release);
    }
  }

  void SyncLock::yield() {
    uint64_t preHolds, newSeed;
    uint32_t tid = Thread::getCurrentTid();
    preHolds = holds;
    holds = 0;
    holder = ~0;
    newSeed = queueSeed.fetch_add(1, std::memory_order_acq_rel);
    queueCurrent.fetch_add(1, std::memory_order_release);
    while (newSeed > queueCurrent.load(std::memory_order_consume)) sleep(1);
    holder = tid;
    holds = preHolds;
  }

  uint64_t SyncLock::getId() {
    return id;
  }
  bool SyncLock::isLocked() {
    return holder == ~0;
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
