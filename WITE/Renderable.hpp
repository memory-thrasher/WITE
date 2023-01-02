#pragma once

#include <vector>

#include "Vulkan.hpp"
#include "ShaderData.hpp"
#include "types.hpp"

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;
  template<acceptShaderData(D)> class Shader;

  class RenderableBase {
  protected:
    RenderableBase(layer_t layer) : layer(layer) {};
  public:
    const layer_t layer;
    virtual void bind(size_t gpu, ElasticCommandBuffer& cmd) = 0;
  };

  template<acceptShaderData(D)> class Renderable : public RenderableBase {
  private:
    Shader<passShaderData(D)>* shader;
    typedef ShaderDescriptor<passShaderData(D), ShaderResourceProvider::eRenderable> data_t;
    data_t data;
  public:
    Renderable(layer_t layer, Shader<passShaderData(D)>* s) : RenderableBase(layer), shader(s) {};
    //TODO function to bind resources to data (statically validated)
    //TODO implement bind
  };

};
