/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <stdlib.h>

#include "syncLock.hpp"
#include "thread.hpp"
#include "profiler.hpp"

namespace WITE {

  syncLock::syncLock() {}

  void syncLock::WaitForLock(bool busy) {
    uint64_t seed;
    seed = queueSeed.fetch_add(1);//take a number
    uint32_t sleepCnt = 0;
    while (seed > queueCurrent.load())
      if(!busy) [[likely]]
	thread::sleepShort(sleepCnt);
  }

  void syncLock::ReleaseLock() {
    queueCurrent.fetch_add(1);
  }

  void syncLock::yield() {
    uint64_t newSeed;
    newSeed = queueSeed.fetch_add(1);
    queueCurrent.fetch_add(1);
    uint32_t sleepCnt = 0;
    while (newSeed > queueCurrent.load()) thread::sleepShort(sleepCnt);
  }

  bool syncLock::isHeld() {
    return queueSeed.load(std::memory_order_consume) > queueCurrent.load(std::memory_order_consume);
  };

  scopeLock::scopeLock(syncLock * sl) {
    sl->WaitForLock(false);//maybe make this optional?
    lock = sl;
    held = true;
  }

  scopeLock::scopeLock(scopeLock&& o) : lock(o.lock), held(o.held) {
    o.lock = NULL;
    o.held = false;
  }

  void scopeLock::release() {
    if(held)
      lock->ReleaseLock();
    held = false;
  };

  scopeLock::~scopeLock() {
    if (held)
      lock->ReleaseLock();
    held = false;
  }

  void scopeLock::yield() {
    lock->yield();
  }

};
