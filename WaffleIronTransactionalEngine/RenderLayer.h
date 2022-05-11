#pragma once

class Renderer;

class RenderLayer {
public:
  RenderLayer();
  ~RenderLayer();
  WITE::renderLayerIdx idx;
  void renderAll(Queue::ExecutionPlan* ep, glm::dmat4* projection);
  void registerShader(Shader* s) { shadersApplicableToLayer.push(s); };//only for Shader init.
private:
  WITE::SyncLock memberLock;
  std::vector<class Renderer*> members;
  std::vector<class Shader*> shadersApplicableToLayer;
  friend class Renderer;
};

#define WITE_RENDERLAYERS_FOR(layerIdx, layerMask, command) for(WITE::renderLayerIdx layerIdx = 0, WITE::renderLayerMask _maskRemnant = layerMask;_maskRemnant;_maskRemnant >>= 1, layerIdx++) if(_maskRemnant & 1) command;

