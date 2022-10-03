#pragma once

#include "Callback.hpp"

#include <set>

namespace WITE::GPU {

  class RenderTarget;

  class Renderer {
  public:
    typedef uint64_t layer_t;
    layer_t layer;
    typedef int64_t stage_t;
    typedefCB(getStages_t, const std::set<stage_t>);
    getStages_t getStages;
    typedefCB(renderStage_t, void, stage_t, RenderTarget&);
    renderStage_t renderStage;
  };

}

