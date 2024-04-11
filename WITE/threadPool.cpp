#include "threadPool.hpp"

namespace WITE {

  threadPool::workerEntry(threadPool* pool, threadPool::threadData_t* threadData) {//static
    threadData->thread = thread::current();
    auto readTarget = threadData->nextRead.load(std::memory_order_relaxed);//won't move till we move it so don't re-read it
    while(!pool->exit.load(std::memory_order_consume)) {
      if(readTarget == threadData->nextWrite.load(std::memory_order_consume)) [[unlikely]] {//queue underflow
	thread::sleepShort();
      } else {
	job_t& j = threadData->jobs[readTarget];
	j.entry(j.data);
	readTarget = (readTarget + 1) & jobMask;
	threadData->nextRead.store(readTarget, std::memory_order_release);
      }
    }
  };

  threadPool::threadPool(uint64_t threadCount) : threadCount(threadCount) {
    threads = std::make_unique<threadData_t[]>(threadCount);
    for(size_t i = 0;i < threadCount;i++) {
      thread::spawnThread(thread::threadEntry_t_F::make(&threads[i], workerEntry));
    }
  };

  threadPool::~threadPool() {
    exit = true;
    for(size_t i = 0;i < threadCount;i++)
      threads[i].thread->join();
  };

  threadPool::submitJob(const job_t* j) {
    scopeLock submitLock(&submitMutex);
    while(true) {
      for(size_t i = 0;i < threadCount;i++) {
	threadData_t& td = threads[i];
	auto writeTarget = td.nextWrite.load(std::memory_order_acquire);
	if((writeTarget + 1) & jobMask != td.nextRead.load(std::memory_order_consume)) [[unlikely]] {
	  //there was room, there will continue to be room since we hold submitLock.
	  td.jobs[writeTarget] = *j;
	  td.nextWrite.fetch_add(1, std::memory_order_release);//don't care about acquire, that's protected by submitLock
	  return;
	}
      }
      thread::sleepShort();
    }
  };

}
