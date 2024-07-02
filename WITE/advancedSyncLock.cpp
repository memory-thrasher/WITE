/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "advancedSyncLock.hpp"
#include "DEBUG.hpp"

namespace WITE {

  advancedSyncLock::advancedSyncLock() {};

  advancedSyncLock::~advancedSyncLock() {
    scopeLock lock(&mutex);
    currentOwner = tid_none();
    holds = (~0)>>1;
  };

  bool advancedSyncLock::acquire(uint64_t timeoutNS) {
    auto endTime = std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeoutNS);
    auto tid = thread::getCurrentTid();
    uint32_t sleepCnt = 0;
    do {
      scopeLock lock(&mutex);
      if(holds == 0 || currentOwner == tid) {
	currentOwner = tid;
	holds++;
	return true;
      }
      lock.release();
      if(timeoutNS > 0)
	thread::sleepShort(sleepCnt);
    } while(std::chrono::steady_clock::now() < endTime);
    return false;
  };

  void advancedSyncLock::release() {
    scopeLock lock(&mutex);
    auto tid = thread::getCurrentTid();
    ASSERT_TRAP(holds > 0, "Mutex hold underflow!");
    ASSERT_TRAP(currentOwner == tid, "Mutex Failure!!! tid: ", tid, " owner: ", currentOwner, " holds: ", holds);
    --holds;
  };

  bool advancedSyncLock::heldBy(tid_t tid) {
    scopeLock lock(&mutex);
    return holds && currentOwner == tid;
  };

  bool advancedSyncLock::isHeld() {
    scopeLock lock(&mutex);
    return holds;
  };

  advancedScopeLock::advancedScopeLock(advancedSyncLock& l, uint64_t timeoutNS) : l(l) {
    reacquire(timeoutNS);
  };

  advancedScopeLock::~advancedScopeLock() {
    if(isHeld())
      release();
  };

  void advancedScopeLock::reacquire(uint64_t timeoutNS) {
    l.acquire(timeoutNS);
  };

  void advancedScopeLock::release() {
    l.release();
  };

};
