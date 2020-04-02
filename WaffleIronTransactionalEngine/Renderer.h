#pragma once

#include "BackedBuffer.h"
#include "Shader.h"
#include "Mesh.h"

class Renderer {
public:
  typedef class Callback_t<void, Renderer*, std::shared_ptr<class ShaderResource>*, GPU*>* packDataCB;//size = count(resources)
  static void bind(Object*, Shader*, VMesh*, renderLayerIdx);
  static void unbind(Object*, renderLayerIdx);
  Renderer();
  ~Renderer();
  //pipeline must be bound first, this should only be called by Shader::render
  void render(glm::mat4d projection, GPU* gpu);
  bool exists() { return obj; };
  void setOnceCallback(packDataCB);
  void setPerFrameCallback(packDataCB);
  void updateInstanceData(size_t resource, GPU* gpu);
private:
  RenderLayer * layer;//TODO use weak_ptr and hand out shared_ptr with getters
  Shader* shader;
  VMesh* mesh;
  Object* obj;
  std::unique_ptr<GPUResource<Shader::Instance>> buffers;
  packDataCB packPreRender, packInitial;
};

