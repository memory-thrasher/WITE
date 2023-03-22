#pragma once

#include <vector>
#include <map>

#include "ShaderData.hpp"
#include "SyncLock.hpp"
#include "FrameBufferedCollection.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "PerGpu.hpp"
#include "ShaderDescriptorLifeguard.hpp"
#include "Renderable.hpp"
#include "RenderTarget.hpp"

namespace WITE::GPU {

  class GpuResource;

  class ShaderBase {
  private:
    static std::atomic<pipelineId_t> idSeed;
    static std::vector<ShaderBase*> allShaders;//all shaders should be created before anything gets rendered, so no syncing
    static PerGpu<std::map<ShaderData::hashcode_t, vk::PipelineLayout>> layoutsByData;
    static Util::SyncLock layoutsByData_mutex;
    std::map<layer_t, Collections::FrameBufferedCollection<RenderableBase*>> renderablesByLayer;
    const QueueType queueType;
    static void renderAllOfTypeTo(RenderTarget&, WorkBatch cmd, QueueType, layerCollection_t&, std::initializer_list<RenderableBase*>& except);
    void renderTo(RenderTarget&, WorkBatch cmd, layerCollection_t&, std::initializer_list<RenderableBase*>& except);
    friend class RenderTarget;
  protected:
    ShaderBase(QueueType qt);
    virtual ~ShaderBase();
    ShaderBase(ShaderBase&) = delete;
    void add(RenderableBase*);
    void remove(RenderableBase*);
    virtual void preRender(RenderTarget& target, WorkBatch cmd, layerCollection_t& layers, size_t gpu) = 0;
    virtual vk::PipelineBindPoint getBindPoint() = 0;
    static bool hasLayout(ShaderData::hashcode_t d, size_t gpuIdx);
    static vk::PipelineLayout getLayout(ShaderData::hashcode_t d, size_t gpuIdx);
    static vk::PipelineLayout makeLayout(ShaderData::hashcode_t d, const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx);
  public:
    virtual vk::Pipeline createPipe(size_t idx, vk::RenderPass rp) = 0;
    const pipelineId_t id = idSeed++;//for RenderTarget to map pipelines against
  };

  template<ShaderData D> class Shader : public ShaderBase {
  private:
    static_assert(D.containsWritable());
    ShaderDescriptor<D, ShaderResourceProvider::eShaderInstance> globalResources;
  protected:
    static constexpr auto DHC = parseShaderData<D>().hashCode();
    Shader(QueueType qt) : ShaderBase(qt) {};
    void preRender(RenderTarget& target, WorkBatch cmd, layerCollection_t& layers, size_t gpu) override {
      vk::DescriptorSet dss[2] = {
	globalResources.get(gpu),
	target.getDescriptor<D>()->get(gpu)
      };
      cmd.bindDescriptorSets(getBindPoint(), getLayout(gpu), 0, 2, dss, 0, NULL);
    };
  public:
    constexpr static auto dataLayout = D;
    static vk::PipelineLayout getLayout(size_t gpuIdx) {
      if(ShaderBase::hasLayout(D.hashCode(), gpuIdx))
	return ShaderBase::getLayout(D.hashCode(), gpuIdx);
      vk::DescriptorSetLayout layouts[3] {
	ShaderDescriptorLifeguard::getDSLayout<D, ShaderResourceProvider::eShaderInstance>(gpuIdx),
	ShaderDescriptorLifeguard::getDSLayout<D, ShaderResourceProvider::eRenderTarget>(gpuIdx),
	ShaderDescriptorLifeguard::getDSLayout<D, ShaderResourceProvider::eRenderable>(gpuIdx)
      };
      vk::PipelineLayoutCreateInfo pipeCI { {}, 3, layouts };
      ShaderBase::makeLayout(DHC, &pipeCI, gpuIdx);
    };
    //an instance-level resource could be a small buffer with a color for tint etc.
    //writable instance-level resources are useless because the instance may be executed to multiple targets in parallel
    template<class... R> inline void setGlobalResources(R... resources) {
      globalResources.SetResources(std::forward<R...>(resources...));
    };
  };

}

#include "ShaderImpl.hpp"


