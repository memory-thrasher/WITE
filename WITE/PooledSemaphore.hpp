#include <memory>

#include "Semaphore.hpp"
#include "PerGpu.hpp"
#include "BalancingThreadResourcePool.hpp"

namespace WITE::GPU {

  //use with shared_ptr to recycle when no longer used
  class PooledSemaphore {//hmm maybe make this generic, wrapper for any BalancingThread
  private:
    static PerGpuUP<WITE::Collections::BalancingThreadResourcePool<Semaphore, 8>> pools;
    size_t gpuIdx;
    Semaphore* semaphore;
  public:
    PooledSemaphore() = default;
    PooledSemaphore(const PooledSemaphore&) = default;
    PooledSemaphore(size_t gpuIdx);
    ~PooledSemaphore();
    void dispose();
    inline operator Semaphore*() { return semaphore; };
    inline bool pending() { return semaphore && semaphore->pending(); };
    inline Semaphore* get() { return semaphore; };
  };

  typedef std::shared_ptr<PooledSemaphore> SharedSemaphore;

#define makeSharedSemaphore(g) std::make_shared<WITE::GPU::PooledSemaphore>(g);

  struct SemaphorePointInTime {
    SharedSemaphore sem;
    uint64_t value;
    inline bool pending() { return sem && sem->get()->getCurrentValue() < value; };
    inline bool reached() { return sem && sem->get()->getCurrentValue() >= value; };
    void create(size_t gpuIdx);
  };

};
