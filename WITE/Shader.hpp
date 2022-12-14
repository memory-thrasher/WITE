#include <vector>
#include <map>

#include "ShaderData.hpp"
#include "StructuralConstList.hpp"
#include "SyncLock.hpp"
#include "FrameBufferedCollection.hpp"
#include "Vulkan.hpp"
#include "types.hpp"
#include "PerGpu.hpp"

namespace WITE::GPU {

  class Renderer;
  class RenderTarget;
  class GpuResource;
  class ElasticCommandBuffer;

  class ShaderBase {
  private:
    static std::atomic<pipelineId_t> idSeed;
    static std::vector<ShaderBase*> allShaders;//all shaders should be created before anything gets rendered, so no syncing
    static PerGpu<std::map<ShaderData, vk::PipelineLayout>> layoutsByData;
    static Util::SyncLock layoutsByData_mutex;
    std::map<layer_t, Collections::FrameBufferedCollection<Renderer*>> renderersByLayer;
    const QueueType queueType;
    static void RenderAllOfTypeTo(RenderTarget&, ElasticCommandBuffer& cmd, QueueType, layerCollection_t&, std::initializer_list<Renderer*>& except);
    void RenderTo(RenderTarget&, ElasticCommandBuffer& cmd, layerCollection_t&, std::initializer_list<Renderer*>& except);
    static vk::PipelineLayout makeLayout(const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx);
    friend class RenderTarget;
  protected:
    ShaderBase(QueueType qt);
    virtual ~ShaderBase();
    ShaderBase(ShaderBase&) = delete;
    void Register(Renderer*);
    void Unregister(Renderer*);
    virtual void RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) = 0;
    virtual vk::PipelineBindPoint getBindPoint() = 0;
    static vk::PipelineLayout getLayout(const ShaderData& d, const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx);
  public:
    virtual vk::Pipeline CreatePipe(size_t idx, vk::RenderPass rp) = 0;
    const pipelineId_t id = idSeed++;//for RenderTarget to map pipelines against
  };

  template<ShaderData D> class Shader : public ShaderBase {
  private:
    static_assert(D.containsWritable());
    ShaderDescriptor<D, ShaderResourceProvider::eShaderInstance> globalResources;
    vk::DescriptorSetLayout layouts[3]; #error << nope. DescriptorLayout is a handle that needs to get allocated PerGpu. Need to hook Lifeguard
    vk::PipelineLayoutCreateInfo pipeCI { {}, 3, layouts };
  protected:
    Shader(QueueType qt) : ShaderBase(qt) {};
    static vk::PipelineLayout getLayout(size_t gpuIdx) { return ShaderBase::getLayout(D, &pipeCI, gpuIdx); };
  public:
    const ShaderData dataLayout = D;
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

