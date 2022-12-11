#pragma once

#include <cstdint>

#include "PerGpu.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "FrameBufferedMap.hpp"
#include "Semaphore.hpp"

namespace WITE::GPU {

  class Renderer;
  class ShaderBase;
  class ElasticCommandBuffer;

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
    static std::vector<std::unique_ptr<RenderTarget>> allRTs;//iterated by truck thread
    static Util::SyncLock allRTs_mutex;
    typedef PerGpu<std::map<pipelineId_t, vk::Pipeline>> pipelinesByShaderId_t;//only one thread may render a RenderTarget at a time (and only once per frame), therefore unsynchronized access is safe
    pipelinesByShaderId_t pipelinesByShaderId;
    typedef PerGpu<vk::RenderPass> renderPasses_t;
    renderPasses_t renderPasses;
    //sem is the semaphore that signals this resources has been updated and (on the next frame) other processes are free to use it
    //truck = copying modified data from one gpu to another. Truck thread constintly watches for signaled tuck semaphores and then copies the modified data to the other gpu(s), signalling the primary semaphores there.
    static void truckThread();
    std::atomic_uint64_t destroyOnFrame;
  protected:
    PerGpu<Semaphore> sem, truckSem;
    //protected data members to be filled by implementing child
    vk::RenderPassCreateInfo2 ci;
    RenderTarget(logicalDeviceMask_t ldm);
    void makeRenderPass(vk::RenderPass*, size_t gpu);
    void destroyRenderPass(vk::RenderPass*, size_t);
    virtual const Collections::StructuralConstList<RenderStep>& getRenderSteps() = 0;
    virtual void getRenderInfo(vk::RenderPassBeginInfo* out, size_t gpuIdx) const = 0;
    friend class ShaderBase;
    virtual void renderQueued(ElasticCommandBuffer& cmd) = 0;//implementation is expected to do something with the rendered image
    virtual void truckResourcesTo(size_t srcGpu, size_t dstGpu) = 0;
  public:
    static void SpawnTruckThread();//internal use only called by gpu init
    RenderTarget() = delete;
    RenderTarget(const RenderTarget&) = delete;
    virtual ~RenderTarget() = default;
    void render(std::initializer_list<Renderer*> except);
    virtual void cull() = 0;
    void destroy();//actual destruction may be deferred to a future frame when nothing is using associated resources
  };

}

