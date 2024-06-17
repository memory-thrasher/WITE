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

  class threadPool {
  public:
    typedef uint64_t jobData_t[4];
    typedefCB(jobEntry_t, void, jobData_t&);
    struct job_t {
      jobEntry_t entry;
      jobData_t data;//because creating a new callback for every job would waste time (malloc)
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
    uint32_t threadCount, nextWorker;
    std::atomic_bool exit = false;

    void workerEntry(threadData_t*);

  public:
    threadPool();
    threadPool(uint64_t threadCount);
    ~threadPool();
    void submitJob(const job_t*);
    void waitForAll();
    bool onMemberThread();

  };

}
