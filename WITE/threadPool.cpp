#include "threadPool.hpp"
#include "configuration.hpp"

namespace WITE {

  void threadPool::workerEntry(threadPool::threadData_t* threadData) {
    threadData->thread = thread::current();
    auto readTarget = threadData->nextRead.load(std::memory_order_relaxed);//won't move till we move it so don't re-read it
    while(!exit.load(std::memory_order_consume)) {
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

  threadPool::threadPool() : threadPool(configuration::getOption("threadsperpool", thread::guessCpuCount())) {};

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
    while(true) {
      for(size_t i = 0;i < threadCount;i++) {
	threadData_t& td = threads[i];
	auto writeTarget = td.nextWrite.load(std::memory_order_acquire);
	if(((writeTarget + 1) & jobMask) != td.nextRead.load(std::memory_order_consume)) [[unlikely]] {
	  //there was room, there will continue to be room since we hold submitLock.
	  td.jobs[writeTarget] = *j;
	  td.nextWrite.store((writeTarget + 1) & jobMask, std::memory_order_release);
	  return;
	}
      }
      thread::sleepShort();
    }
  };

  void threadPool::waitForAll() {
    ASSERT_TRAP(!onMemberThread(), "cannot wait a thread pool from one of its members.");
    for(size_t i = 0;i < threadCount;i++) {
      threadData_t& td = threads[i];
      while(td.nextWrite.load(std::memory_order_consume) !=
	    td.nextRead.load(std::memory_order_consume))//not empty
	thread::sleepShort();
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
