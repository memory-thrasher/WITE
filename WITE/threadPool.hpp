#pragma once

#include "thread.hpp"
#include "syncLock.hpp"

namespace WITE {

  class threadPool {
  public:
    typedef uint64_t jobData_t[4];
    typedefCB(jobEntry_t, void, jobData_t&);
    struct job_t {
      jobEntry_t entry;
      jobData_t data;
    };

  private:
    static constexpr size_t jobQueueSize = 1 << 6, jobMask = jobQueueSize-1;

    struct threadData_t {
      job_t jobs[jobQueueSize];
      std::atomic_uint64_t nextRead = 0, nextWrite = 0;//equal means empty
      thread* thread;
    };

    syncLock submitMutex;//lock only required for submit, read cannot overflow and sporadic underflow is harmless
    std::unique_ptr<threadData_t[]> threads;
    uint32_t threadCount;
    std::atomic_bool exit = false;

    static void workerEntry(threadPool*, threadData_t*);

  public:
    threadPool(uint64_t threadCount);
    ~threadPool();
    submitJob(const job_t*);

  };

}
