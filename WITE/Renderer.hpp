#pragma once

#include "Callback.hpp"

#include <set>

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;

  class Renderer {
  private:
    std::vector<GpuResource*> resources;//null for anything not specific to the renderer instance (attachments and globals
  public:
    const layer_t layer;
  };

}

