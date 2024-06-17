/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "thread.hpp"
#include "syncLock.hpp"

namespace WITE {

  class advancedSyncLock {
  private:
    syncLock mutex;
    tid_t currentOwner;
    uint32_t holds;
  public:
    advancedSyncLock();
    ~advancedSyncLock();
    bool acquire(uint64_t timeoutNS = ~0);//0 means now or never. Execution maytake much longer than specified timeout.
    void release();
    bool heldBy(tid_t tid);
    bool isHeld();
    bool inline isHeldByThisThread() { return heldBy(thread::getCurrentTid()); };
  };

  class advancedScopeLock {
  private:
    advancedSyncLock& l;
  public:
    advancedScopeLock(advancedSyncLock& l, uint64_t timeoutNS = ~0);
    ~advancedScopeLock();
    void release();
    void reacquire(uint64_t timeoutNS = ~0);
    inline bool isHeld() { return l.isHeldByThisThread(); };//must check if timeout specified!
  };

};

