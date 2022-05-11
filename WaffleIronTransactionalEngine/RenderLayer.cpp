#include "Internal.h"


RenderLayer::RenderLayer() {}


RenderLayer::~RenderLayer() {}


void RenderLayer::renderAll(Queue::ExecutionPlan* ep, glm::dmat4* projection, GPU* gpu) {
  for(auto shader : shadersApplicableToLayer) {
    shader->render(ep, projection);
  }
}

