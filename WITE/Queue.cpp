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

  struct qSubmitStaging_t {
    std::vector<vk::SemaphoreSubmitInfo> waits, signals;
  };

  Platform::ThreadResource<qSubmitStaging_t> qSubmitStaging;

  void Queue::submit(Cmd* cmd, uint64_t frame) {
    qSubmitStaging_t* staging = qSubmitStaging.get();
    staging->waits.clear();
    staging->waits.reserve(cmd->waits.size());
    if(frame > 0) {//first frame does not wait
      for(Semaphore* s : cmd->waits) {
	uint64_t waitedFrame = frame - 1;
	if(waitedFrame > 0 && s->getPromisedValue() < waitedFrame)
	  WARN("BUSY WAIT: Semaphore waited on frame ", waitedFrame, " with no pending promise beyond frame ", s->getPromisedValue());
	while(s->getPromisedValue() < waitedFrame);
	//^^possible if truck thread hasn't queued transfer yet
	staging->waits.emplace_back(s->vkSem(), waitedFrame, vk::PipelineStageFlagBits2::eAllCommands, 0);
      }
    }
    staging->signals.clear();
    staging->signals.reserve(cmd->signals.size());
    for(Semaphore* s : cmd->signals) {
      s->notePromise(frame);
      staging->signals.emplace_back(s->vkSem(), frame, vk::PipelineStageFlagBits2::eAllCommands, 0);
    }
    vk::CommandBufferSubmitInfo csi { cmd->cmd };
    vk::SubmitInfo2 si { {}, static_cast<uint32_t>(staging->waits.size()), staging->waits.data(),
      1, &csi, static_cast<uint32_t>(staging->signals.size()), staging->signals.data() };
    size_t qIdx = Platform::Thread::getCurrentTid() % queueInstanceCount;
    {
      Util::ScopeLock lock(&queueMutexes[qIdx]);
      VK_ASSERT(queueInstances[qIdx].submit2(1, &si, VK_NULL_HANDLE), "Queue submission failed");
    };
    cmd->waits.clear();
    cmd->signals.clear();
    pools.get()->cmdPool.free(cmd);
  };

  ElasticCommandBuffer Queue::createBatch(uint64_t frame) {
    return { this, frame };
  };

  ElasticCommandBuffer Queue::createBatch() {
    return { this, Util::FrameCounter::getFrame() };
  };

  // vk::CommandBuffer Queue::getNext() {
  //   return nullptr;
  // };

}
