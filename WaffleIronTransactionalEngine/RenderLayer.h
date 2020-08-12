#pragma once

#include "Export.h"

class Renderer;

class RenderLayer {
public:
  RenderLayer();
  ~RenderLayer();
  WITE::renderLayerIdx idx;
private:
  WITE::SyncLock memberLock;
  std::vector<class Renderer*> members;
  friend class Renderer;
};

