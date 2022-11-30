#pragma once

#include <cstdint>

#include "PerGpu.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "FrameBufferedMap.hpp"

namespace WITE::GPU {

  class Renderer;
  class ShaderBase;

  struct RenderStep {
    layerCollection_t layers;
    QueueType type;
    //TODO some way to distinguish ray tracing and other bind points from generic graphics
    Collections::StructuralConstList<usage_t> resourceUsages;//sensical subset of ImageBase::USAGE_*
    RenderStep(layerCollection_t layers, QueueType type, std::initializer_list<usage_t> usages) :
      layers(layers), type(type), resourceUsages(usages) {};
  };

  class RenderTarget {
  public:
    const logicalDeviceMask_t ldm;
  private:
    typedef PerGpu<std::map<pipelineId_t, vk::Pipeline>> pipelinesByShaderId_t;//only one thread may render a RenderTarget at a time (and only once per frame), therefore unsynchronized access is safe
    pipelinesByShaderId_t pipelinesByShaderId;
    typedef PerGpu<vk::RenderPass> renderPasses_t;
    renderPasses_t renderPasses;
  protected:
    //protected data members to be filled by implementing child
    vk::RenderPassCreateInfo2 ci;
    RenderTarget(logicalDeviceMask_t ldm);
    vk::RenderPass makeRenderPass(size_t gpu);
    void destroyRenderPass(vk::RenderPass&, size_t);
    virtual const Collections::StructuralConstList<RenderStep>& getRenderSteps() = 0;
    virtual void getRenderInfo(vk::RenderPassBeginInfo* out, size_t gpuIdx) const = 0;
    friend class ShaderBase;
  public:
    RenderTarget() = delete;
    RenderTarget(const RenderTarget&) = delete;
    void render(std::initializer_list<Renderer*> except);
  };

}

