#pragma once

#include <vector>
#include <map>

#include "ShaderData.hpp"
#include "StructuralConstList.hpp"
#include "SyncLock.hpp"
#include "FrameBufferedCollection.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "PerGpu.hpp"
#include "ShaderDescriptorLifeguard.hpp"
#include "Renderable.hpp"
#include "RenderTarget.hpp"
#include "ElasticCommandBuffer.hpp"

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
    static void renderAllOfTypeTo(RenderTarget&, ElasticCommandBuffer& cmd, QueueType, layerCollection_t&, std::initializer_list<RenderableBase*>& except);
    void renderTo(RenderTarget&, ElasticCommandBuffer& cmd, layerCollection_t&, std::initializer_list<RenderableBase*>& except);
    friend class RenderTarget;
  protected:
    ShaderBase(QueueType qt);
    virtual ~ShaderBase();
    ShaderBase(ShaderBase&) = delete;
    void add(RenderableBase*);
    void remove(RenderableBase*);
    virtual void preRender(RenderTarget& target, ElasticCommandBuffer& cmd, layerCollection_t& layers, size_t gpu) = 0;
    virtual vk::PipelineBindPoint getBindPoint() = 0;
    static bool hasLayout(ShaderData::hashcode_t d, size_t gpuIdx);
    static vk::PipelineLayout getLayout(ShaderData::hashcode_t d, size_t gpuIdx);
    static vk::PipelineLayout makeLayout(ShaderData::hashcode_t d, const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx);
  public:
    virtual vk::Pipeline createPipe(size_t idx, vk::RenderPass rp) = 0;
    const pipelineId_t id = idSeed++;//for RenderTarget to map pipelines against
  };

  template<acceptShaderData(D)> class Shader : public ShaderBase {
  private:
    static_assert(D.containsWritable());
    ShaderDescriptor<passShaderData(D), ShaderResourceProvider::eShaderInstance> globalResources;
  protected:
    static constexpr auto DHC = parseShaderData<D>().hashCode();
    Shader(QueueType qt) : ShaderBase(qt) {};
    static vk::PipelineLayout getLayout(size_t gpuIdx) {
      if(ShaderBase::hasLayout(D, gpuIdx))
	return ShaderBase::getLayout(D, gpuIdx);
      vk::DescriptorSetLayout layouts[3] {
	ShaderDescriptorLifeguard::getDSLayout<passShaderData(D), ShaderResourceProvider::eShaderInstance>(gpuIdx),
	ShaderDescriptorLifeguard::getDSLayout<passShaderData(D), ShaderResourceProvider::eRenderTarget>(gpuIdx),
	ShaderDescriptorLifeguard::getDSLayout<passShaderData(D), ShaderResourceProvider::eRenderable>(gpuIdx)
      };
      vk::PipelineLayoutCreateInfo pipeCI { {}, 3, layouts };
      ShaderBase::makeLayout(DHC, &pipeCI, gpuIdx);
    };
    void preRender(RenderTarget& target, ElasticCommandBuffer& cmd, layerCollection_t& layers, size_t gpu) override {
      vk::DescriptorSet dss[2] = {
	globalResources.get(gpu),
	target.getDescriptor<passShaderData(D)>()->get(gpu)
      };
      cmd->bindDescriptorSets(getBindPoint(), 0, 2, dss, 0, NULL);
    };
  public:
    constexpr static auto dataLayout = D;
    //an instance-level resource could be a small buffer with a color for tint etc.
    //writable instance-level resources are useless because the instance may be executed to multiple targets in parallel
    template<class... R> inline void setGlobalResources(R... resources) {
      globalResources.SetResources(std::forward<R...>(resources...));
    };
  };

}

#include "ShaderImpl.hpp"


