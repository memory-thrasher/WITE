#include "RenderTarget.hpp"
#include "Gpu.hpp"
#include "Vulkan.hpp"
#include "DEBUG.hpp"
#include "ElasticCommandBuffer.hpp"
#include "Shader.hpp"
#include "Thread.hpp"
#include "FrameCounter.hpp"

namespace WITE::GPU {

  std::vector<std::unique_ptr<RenderTarget>> RenderTarget::allRTs;//iterated by truck thread
  Util::SyncLock RenderTarget::allRTs_mutex;

  RenderTarget::RenderTarget(logicalDeviceMask_t ldm) :
    ldm(ldm),
    renderPasses(renderPasses_t::creator_t_F::make(this, &RenderTarget::makeRenderPass),
		 renderPasses_t::destroyer_t_F::make(this, &RenderTarget::destroyRenderPass)),
    sem(Semaphore::factory),
    truckSem(Semaphore::factory)
  {
    Util::ScopeLock lock(&allRTs_mutex);
    allRTs.emplace_back(this);
  };

  void RenderTarget::makeRenderPass(vk::RenderPass* ret, size_t idx) {
    VK_ASSERT(Gpu::get(idx).getVkDevice().createRenderPass2(&ci, ALLOCCB, ret), "Failed to make render pass");
  };

  void RenderTarget::destroyRenderPass(vk::RenderPass* rp, size_t idx) {
    Gpu::get(idx).getVkDevice().destroyRenderPass(*rp, ALLOCCB);
  };

  void RenderTarget::render(std::initializer_list<RenderableBase*> except) {
    lastRendered = Util::FrameCounter::getFrame();
    auto steps = getRenderSteps();
    Gpu* gpu = Gpu::getGpuFor(ldm);
    size_t gpuIdx = gpu->getIndex();
    Queue* lastQueue, *queue;
    ElasticCommandBuffer cmd;
    vk::RenderPassBeginInfo rpbi;
    for(const RenderStep& step : steps) {
      queue = gpu->getQueue(step.type);
      if(lastQueue != queue) {
	if(lastQueue) {
	  CRASH("NOT YET IMPLEMENTED: render pass containing both graphics and compute steps, on a gpu that does not have a queue family that supports both (must split render pass between queues, or make multiple RPs)");
	} else {
	  cmd = queue->createBatch();
	  getRenderInfo(&rpbi, gpuIdx);
	  cmd->beginRenderPass(&rpbi, vk::SubpassContents::eInline);
	}
	// cmd = queue->createBatch(lastQueue ? &cmd : NULL);
      } else {
	cmd->nextSubpass(vk::SubpassContents::eInline);
      }
      ShaderBase::renderAllOfTypeTo(*this, cmd, step.type, step.layers, except);
      lastQueue = queue;
    }
    if(lastQueue) {
      cmd->endRenderPass();
      renderQueued(cmd);
    }
  };

  void RenderTarget::truckThread() {//static
    // std::vector<vk::Semaphore> waitTargets;//backburnering this cuz waitAny requires all sems be on the same device, yielding busy wait is better
    // std::vector<uint64_t> waitValues;
    while(Gpu::running) {
      auto frame = Util::FrameCounter::getFrame();
      // waitTargets.clear();
      {
	Util::ScopeLock lock(&allRTs_mutex);
	std::erase_if(allRTs, [frame](auto& rt) {//erase calls destructor
	  uint64_t expiration = rt->destroyOnFrame;
	  return expiration && expiration <= frame;
	});
	for(auto& rt : allRTs) {
	  size_t maxTruckIdx = 0;
	  uint64_t minPromise = ~0, maxTruck = 0;
	  Collections::BitmaskIterator devices = Gpu::gpusForLdm(rt->ldm);
	  for(size_t gpuIdx : devices) {
	    uint64_t truckValue = rt->truckSem[gpuIdx].getCurrentValue();
	    uint64_t promiseS = rt->sem[gpuIdx].getPromisedValue();
	    if(promiseS < minPromise) {
	      minPromise = promiseS;
	    }
	    if(truckValue > maxTruck) {
	      maxTruckIdx = gpuIdx;
	      maxTruck = truckValue;
	    }
	  }
	  if(maxTruck > minPromise) {
	    for(size_t gpuIdx : devices) {
	      if(gpuIdx == maxTruckIdx || rt->sem[gpuIdx].getCurrentValue() >= maxTruck) continue;
	      rt->truckResourcesTo(maxTruckIdx, gpuIdx);
	      rt->sem[gpuIdx].signalNow(maxTruck);
	      rt->truckSem[gpuIdx].signalNow(maxTruck);
	    }
	  }
	}
      };
      // size_t cnt = waitTargets.size();
      // if(cnt) {
      // 	waitValues.clear();
      // 	waitValues.reserve(cnt);
      // 	for(size_t i = 0;i < cnt;i++)
      // 	  waitValues.push(frame);
      // 	vk::SemaphoreWaitInfo wi { vk::SemaphoreWaitFlagBits::eAny, cnt, waitTargets.data(), waitValues.data() };
      //   waitAll();
      // 	//TODO vk semaphore bulk wait with any bit
      // } else
      Platform::Thread::sleepShort();
    }
  };

  void RenderTarget::spawnTruckThread() {//static
    Platform::Thread::spawnThread(Platform::Thread::threadEntry_t_F::make(&truckThread));
  };

  void RenderTarget::destroy() {
    destroyOnFrame = FrameCounter::getFrame()+1;
  };

}
