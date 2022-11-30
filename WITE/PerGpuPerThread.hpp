#pragma once

#include <array>
#include <bitset>

#include "Thread.hpp"
#include "constants.hpp"
#include "SyncLock.hpp"
#include "DEBUG.hpp"

namespace WITE::Collections {

  template<class T> class PerGpuPerThread {
    static_assert(sizeof(T) < 4*sizeof(T*));
  public:
    typedefCB(creator_t, void, T*, size_t);//size is gpu idx
    typedefCB(destroyer_t, void, T&, size_t);
  private:
    static constexpr size_t capacity = MAX_GPUS * Platform::MAX_THREADS;
    std::array<T, capacity> data;
    std::bitset<capacity> allocationMask;
    Util::SyncLock allocationLock;
    creator_t creator;
    destroyer_t destroyer;
    static constexpr size_t getIdx(size_t gpuId, uint32_t threadId) { return threadId * MAX_GPUS + gpuId; };
    static constexpr size_t idxToGpu(size_t idx) { return idx % MAX_GPUS; };
  public:
    PerGpuPerThread(creator_t c, destroyer_t d = NULL) : creator(c), destroyer(d) {};

    T& operator[](size_t gpu) {
      uint32_t tid = Platform::Thread::getCurrentTid();
      ASSERT_TRAP(gpu < MAX_GPUS, "bad gpu idx");
      ASSERT_TRAP(tid < Platform::MAX_THREADS, "bad tid");
      size_t idx = getIdx(gpu, tid);
      if(creator && !allocationMask[idx]) {
	Util::ScopeLock lock(&allocationLock);
	if(!allocationMask[idx]) {
	  creator(&data[idx], gpu);
	  allocationMask[idx] = true;
	}
      }
      return data[idx];
    };

    void destroy(size_t gpu) {
      size_t idx = getIdx(gpu, Platform::Thread::getCurrentTid());
      Util::ScopeLock lock(&allocationLock);
      if(allocationMask[idx]) {
	destroyer(data[idx], gpu);
	allocationMask[idx] = false;
      }
    };

    ~PerGpuPerThread() {
      Util::ScopeLock lock(&allocationLock);
      for(size_t i = 0;i < capacity;i++)
	if(allocationMask[i])
	  destroyer(data[i], idxToGpu(i));
    };
  };

}
