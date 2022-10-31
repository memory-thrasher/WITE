#include <vector>
#include <map>

#include "ShaderData.hpp"
#include "StructuralConstList.hpp"
#include "SyncLock.hpp"
#include "FrameBufferedCollection.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  class Renderer;
  class RenderTarget;
  class GpuResource;
  class ElasticCommandBuffer;

  class ShaderBase {
  private:
    static std::vector<ShaderBase*> allShaders;//all shaders should be created before anything gets rendered, so no syncing
    std::map<layer_t, Util::FrameBufferedCollection<Renderer*>> renderersByLayer;
    Util::PerGpu<vk::Pipeline> pipeline;
    const QueueType queueType;
  protected:
    ShaderBase(QueueType qt) : queueType(qt);
    ~ShaderBase();
    ShaderBase(ShaderBase&) = delete;
    void Register(Renderer*);
    void Unregister(Renderer*);
    void DestroyPipe(vk::Pipeline, size_t idx);
    virtual vk::Pipeline CreatePipe(size_t idx) = 0;
    virtual void RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) = 0;
    virtual void BindImpl(RenderTarget&, ElasticCommandBuffer& cmd) = 0;
  public:
    static void Prewarm();//TODO call from Gpu::init after everything is created to create all on-demand objects
    static void RenderAllTo(RenderTarget&, const layerCollection_t&, std::initializer_list<Renderer*> except = {});
    void RenderTo(RenderTarget&, const layerCollection_t&, ElasticCommandBuffer& cmd, std::initializer_list<Renderer*> except);
  };

  template<ShaderData D> class Shader : public ShaderBase {
  public:
    template<size_t idx, class R1> inline void CheckResource(R1) {
      static_assert(D[idx].accepts<R1>());
    };
    template<size_t idx> inline void CheckResource<std::nullptr_t>(std::nullptr_t) {};
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

