#include <vector>
#include <map>

#include "ShaderData.hpp"
#include "Gpu.hpp"
#include "StructuralConstList.hpp"

namespace WITE::GPU {

  class Renderer;
  class RenderTarget;
  class GpuResource;

  class ShaderBase {
  private:
    static std::vector<ShaderBase*> allShaders;
    std::map<layer_t, std::vector<Renderer*>> renderersByLayer;//TODO synchronize this somehow
  protected:
    ShaderBase() = default;
    ShaderBase(ShaderBase&) = delete;
    void Register(Renderer*);
    void Unregister(Renderer*);
  public:
    void RenderAllTo(RenderTarget&, layerCollection_t);
    void RenderTo(RenderTarget&, layerCollection_t);
  };

  template<ShaderData D> class Shader : ShaderBase {
    static_assert(D.containsWritable());
    std::vector<GpuResource*> resources;//TODO make GpuResource, base class for image, buffer and n-buffer
    template<class R1> void AddResource(size_t idx, R1 resource) {
      static_assert(D[idx].accepts<R1>());
      resources.push_back(resource);
    };
    template<class R1, class R2, class... R> void AddResource(size_t idx, R1 r1, R2 r2, R... resources) {
      AddResource(r1);
      AddResource(r2, std::forward<R...>(resources...));
    };
  public:
    const ShaderData dataLayout = D;
    template<class... R> void Register(Renderer* r, R... resources) {//all shaders must have 1 resource (if not what's the point)
      AddResource(0, std::forward<R...>(resources...));
      Register(r);
    };
  };

  template<ShaderData D> class VertexShader : Shader<D> {
    static_assert(D.contains(ShaderData::ColorAttachment));
    static_assert(D.containsOnly({
	  ShaderStage::eDraw,
	  ShaderStage::eAssembler,
	  ShaderStage::eVertex,
	  ShaderStage::eTessellation,
	  ShaderStage::eGeometry,
	  ShaderStage::eFragmentDepth,
	  ShaderStage::eFragment,
	  ShaderStage::eBlending
	}));
    //TODO public constructor that asks for modules for every stage
  };

  template<ShaderData D> class MeshShader : Shader<D> {
    static_assert(D.containsOnly({
	  ShaderStage::eTask,
	  ShaderStage::eMesh,
	  ShaderStage::eFragmentDepth,
	  ShaderStage::eFragment,
	  ShaderStage::eBlending
	}));
    //TODO public constructor that asks for modules for every stage
  };

  template<ShaderData D> class ComputeShader : Shader<D> {
    static_assert(D.containsOnly({ ShaderStage::eCompute }));
    //TODO public constructor that asks for modules for every stage
  };

}
