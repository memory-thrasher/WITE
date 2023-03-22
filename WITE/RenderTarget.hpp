#pragma once

#include <cstdint>

#include "PerGpu.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "FrameBufferedMap.hpp"
#include "ShaderData.hpp"
#include "ShaderDescriptorLifeguard.hpp"
#include "IteratorWrapper.hpp"
#include "WorkBatch.hpp"
#include "LiteralList.hpp"

namespace WITE::GPU {

  class RenderableBase;
  class ShaderBase;

  struct RenderStep {
    layerCollection_t layers;
    QueueType type;
    //TODO some way to distinguish ray tracing and other bind points from generic graphics
    Collections::LiteralList<usage_t> resourceUsages;//sensical subset of ImageBase::USAGE_*
    constexpr RenderStep(layerCollection_t layers, QueueType type, Collections::LiteralList<usage_t> usages) :
      layers(layers), type(type), resourceUsages(usages) {};
    constexpr RenderStep(const RenderStep& o) = default;
    constexpr ~RenderStep() = default;
  };

  class RenderTarget {
  public:
    const logicalDeviceMask_t ldm;
  private:
    typedef PerGpu<std::map<pipelineId_t, vk::Pipeline>> pipelinesByShaderId_t;//only one thread may render a RenderTarget at a time (and only once per frame), therefore unsynchronized access is safe
    pipelinesByShaderId_t pipelinesByShaderId;
    std::map<ShaderData::hashcode_t, std::unique_ptr<ShaderDescriptorBase>> descriptorSets;
  protected:
    typedef PerGpu<vk::RenderPass> renderPasses_t;
    renderPasses_t renderPasses;
    uint64_t lastRendered = 0;
    vk::RenderPassCreateInfo2 ci; //protected data members to be filled by implementing child constructor
    RenderTarget(logicalDeviceMask_t ldm);
    void makeRenderPass(vk::RenderPass*, size_t gpu);
    void destroyRenderPass(vk::RenderPass*, size_t);
    virtual const Collections::IteratorWrapper<const RenderStep*> getRenderSteps() const = 0;
    virtual void getRenderInfo(WorkBatch::renderInfo* out, size_t gpuIdx) = 0;
    friend class ShaderBase;
    virtual void renderQueued(WorkBatch cmd) = 0;//implementation is expected to do something with the rendered image
    virtual void bindResourcesTo(ShaderDescriptorBase*) = 0;
    template<ShaderData SD> ShaderDescriptorBase* getDescriptor() {
      constexpr auto hc = SD.hashCode();
      if(!descriptorSets.contains(hc)) {
	ShaderDescriptorBase* sd = new ShaderDescriptor<SD, ShaderResourceProvider::eRenderTarget>();
	descriptorSets.emplace(hc, sd);
	bindResourcesTo(sd);
	return sd;
      } else
	return descriptorSets.at(hc).get();
    };
  public:
    RenderTarget() = delete;
    RenderTarget(const RenderTarget&) = delete;
    virtual ~RenderTarget() = default;
    void render(std::initializer_list<RenderableBase*> except);
    virtual void cull() = 0;
  };

}

