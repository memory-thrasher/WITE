#include "RenderTarget.hpp"
#include "Gpu.hpp"
#include "Vulkan.hpp"
#include "DEBUG.hpp"
#include "ElasticCommandBuffer.hpp"
#include "Shader.hpp"

namespace WITE::GPU {

  RenderTarget::RenderTarget(logicalDeviceMask_t ldm) :
    ldm(ldm),
    renderPasses(renderPasses_t::creator_t_F::make(this, &RenderTarget::makeRenderPass),
		 renderPasses_t::destroyer_t_F::make(this, &RenderTarget::destroyRenderPass)) {};

  vk::RenderPass RenderTarget::makeRenderPass(size_t idx) {
    vk::RenderPass ret;
    VK_ASSERT(Gpu::get(idx).getVkDevice().createRenderPass2(&ci, ALLOCCB, &ret), "Failed to make render pass");
    return ret;
  };

  void RenderTarget::destroyRenderPass(vk::RenderPass& rp, size_t idx) {
    Gpu::get(idx).getVkDevice().destroyRenderPass(rp, ALLOCCB);
  };

  void RenderTarget::render(std::initializer_list<Renderer*> except) {
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
      ShaderBase::RenderAllOfTypeTo(*this, cmd, step.type, step.layers, except);
      lastQueue = queue;
    }
    if(lastQueue)
      cmd->endRenderPass();
  };

}
