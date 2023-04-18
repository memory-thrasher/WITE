#pragma once

#include <array>
#include <bitset>

#include "CopyableArray.hpp"
#include "Thread.hpp"
#include "constants.hpp"
#include "SyncLock.hpp"
#include "DEBUG.hpp"

namespace WITE::Collections {

  template<class T> class PerGpuPerThread {
    static_assert(sizeof(T) <= 2*sizeof(T*));
  public:
    typedefCB(creator_t, void, T*, size_t);//size is gpu idx
    typedefCB(destroyer_t, void, T*, size_t);
  private:
    static constexpr size_t capacity = MAX_GPUS * MAX_THREADS;
    Collections::CopyableArray<T, capacity> data;
    std::bitset<capacity> allocationMask;
    Util::SyncLock allocationLock;
    creator_t creator;
    destroyer_t destroyer;
    static constexpr size_t getIdx(size_t gpuId, uint32_t threadId) { return threadId * MAX_GPUS + gpuId; };
    static constexpr size_t idxToGpu(size_t idx) { return idx % MAX_GPUS; };
  public:
    PerGpuPerThread(creator_t c = NULL, destroyer_t d = NULL) : creator(c), destroyer(d) {};

    T& operator[](size_t gpu) {
      uint32_t tid = Platform::Thread::getCurrentTid();
      ASSERT_TRAP(gpu < MAX_GPUS, "bad gpu idx");
      ASSERT_TRAP(tid < MAX_THREADS, "bad tid");
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
	if(destroyer)
	  destroyer(&data[idx], gpu);
	allocationMask[idx] = false;
      }
    };

    ~PerGpuPerThread() {
      Util::ScopeLock lock(&allocationLock);
      for(size_t i = 0;i < capacity;i++)
	if(allocationMask[i])
	  destroyer(&data[i], idxToGpu(i));
    };
  };

  template<class T> class PerGpuPerThread<std::unique_ptr<T>> {
  public:
    typedefCB(creator_t, void, T*, size_t);//size is gpu idx
    typedefCB(destroyer_t, void, T*, size_t);
    typedef std::unique_ptr<T> U;
  private:
    static constexpr size_t capacity = MAX_GPUS * MAX_THREADS;
    Collections::CopyableArray<U, capacity> data;
    std::bitset<capacity> allocationMask;
    Util::SyncLock allocationLock;
    creator_t creator;
    destroyer_t destroyer;
    static constexpr size_t getIdx(size_t gpuId, uint32_t threadId) { return threadId * MAX_GPUS + gpuId; };
    static constexpr size_t idxToGpu(size_t idx) { return idx % MAX_GPUS; };
  public:
    PerGpuPerThread(creator_t c = NULL, destroyer_t d = NULL) : creator(c), destroyer(d) {};

    T* operator[](size_t gpu) {
      uint32_t tid = Platform::Thread::getCurrentTid();
      ASSERT_TRAP(gpu < MAX_GPUS, "bad gpu idx");
      ASSERT_TRAP(tid < MAX_THREADS, "bad tid");
      size_t idx = getIdx(gpu, tid);
      if(!allocationMask[idx]) {
	Util::ScopeLock lock(&allocationLock);
	if(!allocationMask[idx]) {
	  if(creator) {
	    data[idx].reset((T*)malloc(sizeof(T)));
	    creator(data[idx].get(), gpu);
	  } else {
	    data[idx] = std::make_unique<T>();
	  }
	  allocationMask[idx] = true;
	}
      }
      return data[idx].get();
    };

    void destroy(size_t gpu) {
      size_t idx = getIdx(gpu, Platform::Thread::getCurrentTid());
      Util::ScopeLock lock(&allocationLock);
      if(allocationMask[idx]) {
	if(destroyer)
	  destroyer(data[idx].get(), gpu);
	allocationMask[idx] = false;
	data[idx].reset();
      }
    };

    ~PerGpuPerThread() {
      Util::ScopeLock lock(&allocationLock);
      if(destroyer)
	for(size_t i = 0;i < capacity;i++)
	  if(allocationMask[i])
	    destroyer(data[i].get(), idxToGpu(i));
    };
  };

}
