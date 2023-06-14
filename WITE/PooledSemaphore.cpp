#include "PooledSemaphore.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  PerGpuUP<WITE::Collections::BalancingThreadResourcePool<Semaphore, 8>> PooledSemaphore::pools;//static

  PooledSemaphore::PooledSemaphore(size_t gpuIdx) : gpuIdx(gpuIdx) {
    semaphore = pools[gpuIdx]->allocate();
    if(!*semaphore)
      new(semaphore)Semaphore(&Gpu::get(gpuIdx));
  };

  PooledSemaphore::~PooledSemaphore() {
    dispose();
  };

  void PooledSemaphore::dispose() {
    if(semaphore && pools[gpuIdx])
      pools[gpuIdx]->free(semaphore);
    semaphore = NULL;
  };

}
