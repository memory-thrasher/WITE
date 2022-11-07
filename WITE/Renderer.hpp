#pragma once

#include <set>

#include "Callback.hpp"
#include "types.hpp"

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;

  class Renderer {
  private:
    std::vector<GpuResource*> resources;//null for anything not specific to the renderer instance (attachments and globals
  public:
    const layer_t layer;
    Renderer(layer_t layer) : layer(layer) {};
  };

}

