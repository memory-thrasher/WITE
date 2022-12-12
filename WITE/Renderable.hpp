#pragma once

#include <vector>

#include "Vulkan.hpp"
#include "ShaderData.hpp"
#include "types.hpp"

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;

 //renderer renders something. renderable renders itself. I need caffeine.
  template<ShaderData D> class Renderable : public Renderer {
  private:
    std::vector<GpuResource*> resources;//null for anything not specific to the renderer instance (attachments and globals
    Shader<D>* shader;
  public:
    const layer_t layer;
    Renderer(layer_t layer, Shader<D>* s) : shader(s), layer(layer) {};
    //TODO function to bind resources (statically validated)
  };

};
