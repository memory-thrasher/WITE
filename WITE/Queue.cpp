#include "Queue.hpp"
#include "Thread.hpp"
#include "Gpu.hpp"
#include "ElasticCommandBuffer.hpp"

namespace WITE::GPU {

  Queue::Queue(Gpu* gpu, const struct vk::DeviceQueueCreateInfo& ci) :
    gpu(gpu),
    queueInstanceCount(ci.queueCount),
    family(ci.queueFamilyIndex),
    queueInstances(std::make_unique<vk::Queue[]>(queueInstanceCount)),
    queueMutexes(std::make_unique<Util::SyncLock[]>(queueInstanceCount)),
    pools(Platform::ThreadResource<PerThreadResources>::Initer_F::make(this, &Queue::makePerThreadResources),
	  Platform::ThreadResource<PerThreadResources>::Destroyer_F::make(this, &Queue::freePerThreadResources))
  {
    auto dev = gpu->getVkDevice();
    // queueInstances = std::make_unique<vk::Queue[]>(queueInstanceCount);
    // queueMutexes = std::make_unique<Util::SyncLock[]>(queueInstanceCount);
    for(size_t i = 0;i < queueInstanceCount;i++)
      dev.getQueue(family, i, &queueInstances[i]);
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
    // for(auto cmd : o->cachedCmds.iterAll()) {
    //TODO if needed by anything else in PerThreadResources::Cmd
    // }
    dev.destroyCommandPool(o->pool);//this frees all cmdbuffers
  };

  // void Queue::submit(ElasticCommandBuffer* buf) {
  //   //TODO
  // };

  // ElasticCommandBuffer Queue::createBatch(uint64_t ldm) {
  //   //TODO
  //   return { this };
  // };

  // vk::CommandBuffer Queue::getNext() {
  //   return nullptr;
  // };

}
