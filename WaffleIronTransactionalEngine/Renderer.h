#pragma once

#include "BackedBuffer.h"
#include "Shader.h"

class Object;
class RenderLayer;
class Mesh;

class Renderer {
public:
  typedef WITE::Callback_t<void, Renderer*, std::shared_ptr<class ShaderResource>*, GPU*>* packDataCB;//size = count(resources)
  static void bind(Object*, Shader*, Mesh*, WITE::renderLayerIdx);
  static void unbind(Object*, WITE::renderLayerIdx);
  Renderer();
  ~Renderer();
  //pipeline must be bound first, this should only be called by Shader::render
  void render(glm::dmat4 projection, GPU* gpu);
  bool exists() { return obj; };
  void setOnceCallback(packDataCB);
  void setPerFrameCallback(packDataCB);
  void updateInstanceData(size_t resource, GPU* gpu);
private:
  RenderLayer * layer;//TODO use weak_ptr and hand out shared_ptr with getters
  Shader* shader;
  Mesh* mesh;
  Object* obj;
  std::unique_ptr<GPUResource<Shader::Instance>> buffers;
  packDataCB packPreRender, packInitial;
};

