#include "cmdPool.hpp"
#include "gpu.hpp"
#include "DEBUG.hpp"

namespace WITE {

  void tempCmd::submit() {
    cmd.end();
    vk::CommandBufferSubmitInfo cmdSubmitInfo(cmd);
    vk::SubmitInfo2 submit({}, 0, NULL, 1, &cmdSubmitInfo, 0, NULL);
    if(pool->dev.getFenceStatus(fence) == vk::Result::eSuccess)
      VK_ASSERT(pool->dev.resetFences(1, &fence), "Failed to reset fence");
    {
      scopeLock lock(pool->qMutex);
      VK_ASSERT(pool->q.submit2(1, &submit, fence), "failed to submit command buffer");
    }
    pool->submitted.push(*this);
  };

  bool tempCmd::isPending() {
    return pool->dev.getFenceStatus(fence) == vk::Result::eNotReady;
  };

  void tempCmd::waitFor() {
    VK_ASSERT(pool->dev.waitForFences(1, &fence, false, 10000000000), "Fence wait timeout (10 seconds)");
  };

  cmdPool::cmdPool(vk::CommandPool pool, vk::Device dev, vk::Queue q, syncLock* qMutex) : pool(pool), dev(dev), q(q), qMutex(qMutex) {};

  cmdPool::~cmdPool() {
    //TODO cleanup
  };

  cmdPool* cmdPool::forDevice(size_t idx) {//static
    gpu* dev = &gpu::get(idx);
    vk::CommandPool pool;
    const vk::CommandPoolCreateInfo poolCI(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, dev->getQueueFam());
    VK_ASSERT(dev->getVkDevice().createCommandPool(&poolCI, ALLOCCB, &pool), "failed to allocate command pool");
    return new cmdPool(pool, dev->getVkDevice(), dev->getQueue(), dev->getQueueMutex());
  };

  cmdPool* cmdPool::lowPrioForDevice(size_t idx) {
    gpu* dev = &gpu::get(idx);
    vk::CommandPool pool;
    const vk::CommandPoolCreateInfo poolCI(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, dev->getQueueFam());
    VK_ASSERT(dev->getVkDevice().createCommandPool(&poolCI, ALLOCCB, &pool), "failed to allocate command pool");
    return new cmdPool(pool, dev->getVkDevice(), dev->getLowPrioQueue(), dev->getLowPrioQueueMutex());
  };

  tempCmd cmdPool::allocate() {
    tempCmd ret;
    if(submitted.empty() || submitted.front().isPending()) {
      ret.pool = this;
      static constexpr vk::FenceCreateInfo fenceCI(vk::FenceCreateFlagBits::eSignaled);
      VK_ASSERT(dev.createFence(&fenceCI, ALLOCCB, &ret.fence), "failed to create fence");
      vk::CommandBufferAllocateInfo allocInfo(pool, vk::CommandBufferLevel::ePrimary, 1);
      VK_ASSERT(dev.allocateCommandBuffers(&allocInfo, &ret.cmd), "failed to allocate command buffer");
    } else {
      ret = submitted.front();
      submitted.pop();
    }
    static constexpr vk::CommandBufferBeginInfo begin { vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    VK_ASSERT(ret->begin(&begin), "failed to begin cmd");
    return ret;
  };

};
