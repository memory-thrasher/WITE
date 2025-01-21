/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "syncLock.hpp"

namespace WITE {

  //allows one write hold with no reads, or many read holds with no write
  class concurentReadSyncLock {
  private:
    syncLock internalMutex;
    bool writeLocked = false;
    uint64_t readLocks = 0;
  public:
    concurentReadSyncLock() = default;
    concurentReadSyncLock(const concurentReadSyncLock&) = delete;
    concurentReadSyncLock(concurentReadSyncLock&&) = delete;
    ~concurentReadSyncLock() = default;
    void acquireRead();
    void releaseRead();
    void acquireWrite();
    void releaseWrite();
    bool isReadHeld();
    bool isWriteHeld();
  };

  class concurrentReadLock_read {
  private:
    concurentReadSyncLock* lock;
  public:
    concurrentReadLock_read() = delete;
    concurrentReadLock_read(const concurrentReadLock_read&) = delete;
    concurrentReadLock_read(concurrentReadLock_read&&) = delete;
    concurrentReadLock_read(concurentReadSyncLock*);
    ~concurrentReadLock_read();
  };

  class concurrentReadLock_write {
  private:
    concurentReadSyncLock* lock;
  public:
    concurrentReadLock_write() = delete;
    concurrentReadLock_write(const concurrentReadLock_write&) = delete;
    concurrentReadLock_write(concurrentReadLock_write&&) = delete;
    concurrentReadLock_write(concurentReadSyncLock*);
    ~concurrentReadLock_write();
  };

}
