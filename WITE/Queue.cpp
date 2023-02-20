#include "Queue.hpp"
#include "Thread.hpp"
#include "Gpu.hpp"
#include "ElasticCommandBuffer.hpp"
#include "DEBUG.hpp"

namespace WITE::GPU {

  Queue::Queue(Gpu* gpu, const struct vk::DeviceQueueCreateInfo& ci, vk::QueueFamilyProperties& qfp) :
    gpu(gpu),
    queueInstanceCount(ci.queueCount),
    queueInstances(std::make_unique<vk::Queue[]>(queueInstanceCount)),
    queueMutexes(std::make_unique<Util::SyncLock[]>(queueInstanceCount)),
    pools(Platform::ThreadResource<PerThreadResources>::Initer_F::make(this, &Queue::makePerThreadResources),
	  Platform::ThreadResource<PerThreadResources>::Destroyer_F::make(this, &Queue::freePerThreadResources)),
    family(ci.queueFamilyIndex),
    qfp(qfp)
  {
    auto dev = gpu->getVkDevice();
    for(size_t i = 0;i < queueInstanceCount;i++)
      dev.getQueue(family, i, &queueInstances[i]);
  };

  Queue::~Queue() {
    //TODO destroy command pools? or does destroying the device do that?
  };

  Queue::PerThreadResources* Queue::makePerThreadResources() {
    auto ret = new PerThreadResources();
    uint32_t i = Platform::Thread::getCurrentTid() % queueInstanceCount;
    ret->idx = i;
    Util::ScopeLock lock(&queueMutexes[i]);
    vk::CommandPoolCreateInfo ci(vk::CommandPoolCreateFlagBits::eTransient |
				 vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				 family);
    VK_ASSERT(gpu->getVkDevice().createCommandPool(&ci, ALLOCCB, &ret->pool), "Failed to create command buffer pool");
    return ret;
  };

  void Queue::freePerThreadResources(PerThreadResources* o) {
    size_t i = o->idx % queueInstanceCount;
    Util::ScopeLock lock(&queueMutexes[i]);
    auto dev = gpu->getVkDevice();
    dev.destroyCommandPool(o->pool);//this frees all cmdbuffers
  };

  struct qSubmitStaging_t {
    std::vector<vk::SemaphoreSubmitInfo> waits, signals;
  };

  Platform::ThreadResource<qSubmitStaging_t> qSubmitStaging;
  ElasticCommandBuffer Queue::createBatch(uint64_t frame) {
    return { this, frame };
  };

  ElasticCommandBuffer Queue::createBatch() {
    return { this, Util::FrameCounter::getFrame() };
  };

  vk::CommandBuffer Queue::getNext() {
    auto ptr = pools.get();
    vk::CommandBuffer* ret = ptr->cmdPool.allocate();
    if(!*ret) {
      vk::CommandBufferAllocateInfo ai { ptr->pool, vk::CommandBufferLevel::ePrimary, 1 };
      VK_ASSERT(gpu->getVkDevice().allocateCommandBuffers(&ai, ret), "failed to allocate command buffer");
    } else {
      cmd->reset({});
    }
    vk::CommandBufferBeginInfo bi { vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    ret->begin(bi);
    return *ret;
  };

  void Queue::submit(vk::CommandBuffer cmd, Semaphore* wait, Semaphore* signal) {
    cmd.end();
    vk::CommandBufferSubmitInfo csi { cmd };
    vk::SemaphoreSubmitInfo w, s;
    if(wait)
      w.setSemaphore(*wait).setValue(wait->getPromisedValue()).setStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
    if(signal)
      s.setSemaphore(*signal).setValue(signal->notePromise()).setStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
    vk::SubmitInfo2 si { {}, wait ? 1 : 0, &w, 1, &csi, signal ? 1 : 0, &s : NULL };
    size_t qIdx = Platform::Thread::getCurrentTid() % queueInstanceCount;
    {
      Util::ScopeLock lock(&queueMutexes[qIdx]);
      VK_ASSERT(queueInstances[qIdx].submit2(1, &si, VK_NULL_HANDLE), "Queue submission failed");
    };
    pools.get()->cmdPool.free(cmd);
  };

  void Queue::present(const vk::PresentInfoKHR* pi) {
    size_t qIdx = Platform::Thread::getCurrentTid() % queueInstanceCount;
    Util::ScopeLock lock(&queueMutexes[qIdx]);
    VK_ASSERT(queueInstances[qIdx].presentKHR(pi), "Queue presentation failed");
  };

}
