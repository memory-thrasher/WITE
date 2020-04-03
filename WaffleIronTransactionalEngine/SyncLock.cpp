#include "stdafx.h"
#include "SyncLock.h"
#include "Globals.h"
#include "Export.h"

/*uint64_t timeNs() {
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  return ((((LONGLONG)ft.dwHighDateTime) << 32LL) + (LONGLONG)ft.dwLowDateTime) * 100;
  //wraparound is ~June 2185;  64 bit uint in nanoseconds
  }*/

SyncLock::mutex_t SyncLock::seed = 0;

SyncLock::SyncLock() : holds(0), id(seed++), holder(0), queueSeed(0), queueCurrent(0) {}

SyncLock::~SyncLock() {}

void SyncLock::WaitForLock() {
  size_t tid = Thread::getCurrentTid(), seed;
  if (holder == tid) return;
  seed = queueSeed++;//take a number
  while (seed > queueCurrent) sleep(1);
}

void SyncLock::ReleaseLock() {
  if (holder != Thread::getCurrentTid()) CRASH;
  holds--;
  if (!holds) {
    holder = 0;
    queueCurrent++;
  }
}

void SyncLock::yield() {
  ReleaseLock();
  WaitForLock();
}

uint64_t SyncLock::getId() {
  return id;
}
bool SyncLock::isLocked() {
  return holder == 0;
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

std::unique_ptr<SyncLock> WITE::SyncLock::make() {
  return std::make_unique<SyncLock>();
}

std::unique_ptr<ScopeLock> WITE::ScopeLock::make(WITE::SyncLock* l) {
  return std::make_unique<ScopeLock>(l);
}
