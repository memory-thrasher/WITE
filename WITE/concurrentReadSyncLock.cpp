/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "concurrentReadSyncLock.hpp"
#include "thread.hpp"

namespace WITE {

  void concurrentReadSyncLock::acquireRead() {
    {
      scopeLock lock(&internalMutex);
      readLocks++;//do this early to prevent a new write from grabbing hold if a new one appears while we wait
      if(!writeLocked) [[likely]] return;
    }
    uint32_t sleepCnt = 0;
    while(true) {
      thread::sleepShort(sleepCnt);
      // scopeLock lock(&internalMutex);//we've already claimed a read, so as soon as the write is clear, we're good to start, no need to ask permission to read the write flag
      if(!writeLocked) [[unlikely]] break;
    }
  };

  void concurrentReadSyncLock::releaseRead() {
    scopeLock lock(&internalMutex);
    readLocks--;
  };

  void concurrentReadSyncLock::acquireWrite() {
    uint32_t sleepCnt = 0;
    while(true) {
      {
	scopeLock lock(&internalMutex);
	if(!writeLocked && readLocks == 0) [[likely]] {
	  //not actually likely, but I optimize for the branch not headed for a wait
	  writeLocked = true;
	  break;
	}
      }
      thread::sleepShort(sleepCnt);
    }
  };

  void concurrentReadSyncLock::releaseWrite() {
    scopeLock lock(&internalMutex);
    writeLocked = false;
  };

  bool concurrentReadSyncLock::isReadHeld() {
    //no mutex bc the answer is outdated instantly anyway, unless the mutex is already held externally
    return readLocks > 0 && !writeLocked;
  };

  bool concurrentReadSyncLock::isWriteHeld() {
    //no mutex bc the answer is outdated instantly anyway, unless the mutex is already held externally
    return writeLocked;
  };

  concurrentReadLock_read::concurrentReadLock_read(concurrentReadSyncLock* l) : lock(l) {
    l->acquireRead();
  };

  concurrentReadLock_read::~concurrentReadLock_read() {
    lock->releaseRead();
  };

  concurrentReadLock_write::concurrentReadLock_write(concurrentReadSyncLock* l) : lock(l) {
    l->acquireWrite();
  };

  concurrentReadLock_write::~concurrentReadLock_write() {
    lock->releaseWrite();
  };

}
