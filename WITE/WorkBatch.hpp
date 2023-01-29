#include <vector>

#include "SyncLock.hpp"
#include "Callback.hpp"
#include "Semaphore.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  class Queue;

  class WorkBatch {
  private:
    enum state_t : uint8_t {
      recording, pending, running, done
    };

    struct Work {
      thenCB cpu;//if this contains both cpu and gpu work, do the cpu work first.
      size_t gpu = ~0;
      Queue* q;
      vk::CommandBuffer cmd;
    };

    struct WorkBatchResources {
      state_t state = state_t::recording;
      Util::SyncLock mutex;
      uint64_t cycle;
      Semaphore completeSem;
      std::vector<Work> steps;
      std::vector<WorkBatch> prerequisits, waiters;//waiters get tested and potentailly triggered when this work is done
      std::vector<vk::SemaphoreSubmitInfo> waitedSemsStaging;
      std::vector<vk::CommandBufferSubmitInfo> cmdStaging;
      std::vector<vk::SubmitInfo2> submitStaging;
      uint64_t currentStep;

      inline bool canRecord();
      Work& getCurrent();
      Work& append();
      void reset();
      bool canStart();
    };

    static BalancingThreadResourcePool<WorkBatchResources> allBatches;

    WorkBatchResources* batch;
    uint64_t cycle;

    inline WorkBatchResources* get();
    inline operator WorkBatchResources*() { return get(); };
    void execute(Work&);
    bool isDone(Work&);

  public:
    WorkBatch(uint64_t gpuIdx = -1);
    WorkBatch(const WorkBatch&) = default;

    typedefCB(thenCB, WorkBatch);

    static void init();

    void check();
    WorkBatch then(thenCB);
    WorkBatch thenOnGpu(size_t gpuIdx, thenCB = NULL);
    WorkBatch thenOnAGpu(logicalDeviceMask_t ldm);
    WorkBatch submit();//begins execution. Some or all may be blocking/synchronous.
    void mustHappenAfter(WorkBatch& p);
    bool isDone() const;//done, null, and recording hasn't started are the same thing
    bool isSubmitted() const;//post-submission states also return true
    bool isWaiting() const;
    operator bool() const { return !isDone(); };
    auto operator <=>() const = default;

    //begin vk::cmd stuff. All return a GpuPromise& because whether a new cmd and promise has to be made is known only at runtime
  };

}
