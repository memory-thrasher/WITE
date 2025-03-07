/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "threadPool.hpp"
#include "configuration.hpp"
#include "math.hpp"

namespace WITE {

  void threadPool::workerEntry(threadPool::threadData_t* threadData) {
    threadData->thread = thread::current();
    auto readTarget = threadData->nextRead.load(std::memory_order_relaxed);//won't move till we move it so don't re-read it
    uint32_t sleepCnt = 0;
    while(!exit.load(std::memory_order_consume)) {
      if(readTarget == threadData->nextWrite.load(std::memory_order_consume)) [[unlikely]] {//queue underflow
	thread::sleepShort(sleepCnt);
      } else {
	sleepCnt = 0;
	job_t& j = threadData->jobs[readTarget];
	j.entry(j.data);
	readTarget = (readTarget + 1) & jobMask;
	threadData->nextRead.store(readTarget, std::memory_order_release);
      }
    }
  };

  threadPool::threadPool() : threadPool(configuration::getOption("threadsperpool", max(1, thread::guessCpuCount()-2))) {};

  threadPool::threadPool(uint64_t threadCount) : threadCount(threadCount) {
    thread::init();//repeated calling not a problem
    threads = std::make_unique<threadData_t[]>(threadCount);
    for(size_t i = 0;i < threadCount;i++) {
      thread::spawnThread(thread::threadEntry_t_F::make<threadPool, threadData_t*>(this, &threads[i], &threadPool::workerEntry));
    }
  };

  threadPool::~threadPool() {
    exit = true;
    for(size_t i = 0;i < threadCount;i++)
      threads[i].thread->join();
  };

  void threadPool::submitJob(const job_t* j) {
    scopeLock submitLock(&submitMutex);
    uint32_t sleepCnt = 0;
    while(true) {
      for(uint32_t k = 0;k < threadCount;k++) {
	uint32_t i = (k + nextWorker) % threadCount;//round robin distributor with sporadic skipping
	threadData_t& td = threads[i];
	auto writeTarget = td.nextWrite.load(std::memory_order_acquire);
	if(((writeTarget + 1) & jobMask) != td.nextRead.load(std::memory_order_consume)) [[unlikely]] {
	  //there was room, there will continue to be room since we hold submitLock.
	  td.jobs[writeTarget] = *j;
	  td.nextWrite.store((writeTarget + 1) & jobMask, std::memory_order_release);
	  nextWorker = (k + 1) % threadCount;
	  return;
	}
      }
      thread::sleepShort(sleepCnt);
    }
  };

  void threadPool::waitForAll() {
    uint32_t sleepCnt = 0;
    ASSERT_TRAP(!onMemberThread(), "cannot wait a thread pool from one of its members.");
    for(size_t i = 0;i < threadCount;i++) {
      threadData_t& td = threads[i];
      while(td.nextWrite.load(std::memory_order_consume) !=
	    td.nextRead.load(std::memory_order_consume))//not empty
	thread::sleepShort(sleepCnt);
    }
  };

  bool threadPool::onMemberThread() {
    auto current = thread::current();
    for(size_t i = 0;i < threadCount;i++) {
      threadData_t& td = threads[i];
      if(current == td.thread)
	return true;
    }
    return false;
  };

}
