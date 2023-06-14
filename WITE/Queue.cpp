#include "Queue.hpp"
#include "Thread.hpp"
#include "Gpu.hpp"
#include "DEBUG.hpp"

namespace WITE::GPU {

  Queue::Queue() : family(0) {};//dummy

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

  cmd_t Queue::getNext() {
    auto ptr = pools.get();
    cmd_t ret = ptr->cmdPool.allocate();
    if(!*ret) {
      vk::CommandBufferAllocateInfo ai { ptr->pool, vk::CommandBufferLevel::ePrimary, 1 };
      VK_ASSERT(gpu->getVkDevice().allocateCommandBuffers(&ai, ret.cmd), "failed to allocate command buffer");
      // LOG("Allocated new cmd ", *ret);
    } else {
      // LOG("Allocated recycled cmd ", *ret);
      ret->reset({});
    }
    vk::CommandBufferBeginInfo bi { vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    ret->begin(bi);
    return ret;
  };

  void Queue::free(cmd_t cmd) {
    // LOG("Freed cmd ", *cmd);
    pools.get()->cmdPool.free(cmd.cmd);
  };

  void Queue::submit(const vk::SubmitInfo2& si) {
    size_t qIdx = Platform::Thread::getCurrentTid() % queueInstanceCount;
    Util::ScopeLock lock(&queueMutexes[qIdx]);
    VK_ASSERT(queueInstances[qIdx].submit2(1, &si, VK_NULL_HANDLE), "Queue submission failed");
  };

  void Queue::present(const vk::PresentInfoKHR* pi) {
    size_t qIdx = Platform::Thread::getCurrentTid() % queueInstanceCount;
    Util::ScopeLock lock(&queueMutexes[qIdx]);
    VK_ASSERT(queueInstances[qIdx].presentKHR(pi), "Queue presentation failed");
  };

  size_t Queue::getGpuIdx() {
    return gpu->getIndex();
  };

}
