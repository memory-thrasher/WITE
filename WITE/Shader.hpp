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

namespace WITE::GPU {

  class Renderer;
  class RenderTarget;
  class GpuResource;
  class ElasticCommandBuffer;

  class ShaderBase {
  private:
    static std::atomic<pipelineId_t> idSeed;
    static std::vector<ShaderBase*> allShaders;//all shaders should be created before anything gets rendered, so no syncing
    static PerGpu<std::map<ShaderData::hashcode_t, vk::PipelineLayout>> layoutsByData;
    static Util::SyncLock layoutsByData_mutex;
    std::map<layer_t, Collections::FrameBufferedCollection<Renderer*>> renderersByLayer;
    const QueueType queueType;
    static void RenderAllOfTypeTo(RenderTarget&, ElasticCommandBuffer& cmd, QueueType, layerCollection_t&, std::initializer_list<Renderer*>& except);
    void RenderTo(RenderTarget&, ElasticCommandBuffer& cmd, layerCollection_t&, std::initializer_list<Renderer*>& except);
    friend class RenderTarget;
  protected:
    ShaderBase(QueueType qt);
    virtual ~ShaderBase();
    ShaderBase(ShaderBase&) = delete;
    void Register(Renderer*);
    void Unregister(Renderer*);
    virtual void RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) = 0;
    virtual vk::PipelineBindPoint getBindPoint() = 0;
    static vk::PipelineLayout getLayout(ShaderData::hashcode_t d, size_t gpuIdx);
    static vk::PipelineLayout hasLayout(ShaderData::hashcode_t d, size_t gpuIdx);
    static vk::PipelineLayout makeLayout(ShaderData::hashcode_t d, const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx);
  public:
    virtual vk::Pipeline CreatePipe(size_t idx, vk::RenderPass rp) = 0;
    const pipelineId_t id = idSeed++;//for RenderTarget to map pipelines against
  };

  template<ShaderData D> class Shader : public ShaderBase {
  private:
    static_assert(D.containsWritable());
    ShaderDescriptor<D, ShaderResourceProvider::eShaderInstance> globalResources;
  protected:
    Shader(QueueType qt) : ShaderBase(qt) {};
    static vk::PipelineLayout getLayout(size_t gpuIdx) {
      if(ShaderBase::hasLayout(D, gpuIdx))
	return ShaderBase::getLayout(D, gpuIdx);
      vk::DescriptorSetLayout layouts[3] {
	ShaderDescriptorLifeguard.getDSLayout<D, ShaderResourceProvider.eShaderInstance>(gpuIdx),
	ShaderDescriptorLifeguard.getDSLayout<D, ShaderResourceProvider.eRenderable>(gpuIdx),
	ShaderDescriptorLifeguard.getDSLayout<D, ShaderResourceProvider.eRenderTarget>(gpuIdx)
      };
      vk::PipelineLayoutCreateInfo pipeCI { {}, 3, layouts };
      ShaderBase::makeLayout(&pipeCI, gpuIdx);
    };
  public:
    constexpr static ShaderData dataLayout = D;
    //an instance-level resource could be a small buffer with a color for tint etc.
    //writable instance-level resources are useless because the instance may be executed to multiple targets in parallel
    template<class... R> inline void SetGlobalResources(R... resources) {
      globalResources.SetResources(std::forward<R...>(resources...));
    };
    template<class... R> void Register(Renderer* r) {
      Register(r);
    };
  };

}

#include "ShaderImpl.hpp"

