#include "RenderTarget.hpp"
#include "Gpu.hpp"
#include "Vulkan.hpp"
#include "DEBUG.hpp"
#include "ElasticCommandBuffer.hpp"
#include "Shader.hpp"
#include "Thread.hpp"
#include "FrameCounter.hpp"

namespace WITE::GPU {

  RenderTarget::RenderTarget(logicalDeviceMask_t ldm) :
    ldm(ldm),
    renderPasses(renderPasses_t::creator_t_F::make(this, &RenderTarget::makeRenderPass),
		 renderPasses_t::destroyer_t_F::make(this, &RenderTarget::destroyRenderPass))
  {};

  void RenderTarget::makeRenderPass(vk::RenderPass* ret, size_t idx) {
    VK_ASSERT(Gpu::get(idx).getVkDevice().createRenderPass2(&ci, ALLOCCB, ret), "Failed to make render pass");
  };

  void RenderTarget::destroyRenderPass(vk::RenderPass* rp, size_t idx) {
    Gpu::get(idx).getVkDevice().destroyRenderPass(*rp, ALLOCCB);
  };

  void RenderTarget::render(std::initializer_list<RenderableBase*> except) {
    lastRendered = Util::FrameCounter::getFrame();
    auto steps = getRenderSteps();
    WorkBatch cmd(ldm);
    WorkBatch::renderInfo ri;
    getRenderInfo(&ri, cmd.getGpuIdx());
    cmd.beginRenderPass(&ri);
    for(const RenderStep& step : steps) {
      queue = gpu->getQueue(step.type);
      if(step.type == QueueType::eCompute && !cmd.getGpu().canGraphicsCompute()) {
	CRASH("NOT YET IMPLEMENTED: render pass containing both graphics and compute steps, on a gpu that does not have a queue family that supports both (must split render pass between queues, or make multiple RPs)");
      } else {
	cmd->nextSubpass(vk::SubpassContents::eInline);
      }
      ShaderBase::renderAllOfTypeTo(*this, cmd, step.type, step.layers, except);
    }
    cmd->endRenderPass();
    renderQueued(cmd);
  };

}
