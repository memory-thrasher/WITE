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
    static vk::PipelineLayout makeLayout(const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx);
  public:
    virtual vk::Pipeline CreatePipe(size_t idx, vk::RenderPass rp) = 0;
    const pipelineId_t id = idSeed++;//for RenderTarget to map pipelines against
  };

  template<ShaderData D> class Shader : public ShaderBase {
  public:
    template<size_t idx, class R1> inline void CheckResource(R1) {
      static_assert(std::is_same<std::nullptr_t, R1>() || D[idx].accepts<R1>());
    };
    template<size_t idx, class R1, class R2, class... R> inline void CheckResource(R1 r1, R2 r2, R... resources) {
      CheckResource<idx>(r1);
      CheckResource<idx + 1>(r2, std::forward<R...>(resources...));
    };
    template<class R1, class... R> inline void CheckResource(R1 r1, R... resources) {
      CheckResource<0>(std::forward<R...>(resources...));
    };
  private:
    static_assert(D.containsWritable());
    std::vector<GpuResource*> globalResources;//for anything all shader instances share
    template<size_t idx, class R1> inline void SetResource(R1 resource) {
      CheckResource<idx>(resource);
      globalResources.push_back(resource);
    };
    template<size_t idx, class R1, class R2, class... R> inline void SetResource(R1 r1, R2 r2, R... resources) {
      SetResource<idx>(r1);
      SetResource<idx + 1>(r2, std::forward<R...>(resources...));
    };
  public:
    const ShaderData dataLayout = D;
    template<class... R> void SetGlobalResources(R... resources) {
      globalResources.clear();
      SetResource<0>(std::forward<R...>(resources...));
    };
    template<class... R> void Register(Renderer* r, R... resources) {//all shaders must have 1 resource (if not what's the point)
      CheckResource<0>(std::forward<R...>(resources...));
      Register(r);
    };
  };

}

#include "ShaderImpl.hpp"

