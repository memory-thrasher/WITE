#pragma once

#include "BackedBuffer.h"
#include "Shader.h"

class Object;
class RenderLayer;
class Mesh;

class Renderer {
public:
  typedef WITE::Callback_t<void, Renderer*, std::shared_ptr<class ShaderResource>*, GPU*>* packDataCB;//size = count(resources)
  static void bind(std::shared_ptr<Object> o, std::shared_ptr<Shader> s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx);
  static void unbind(std::shared_ptr<Object>, WITE::renderLayerIdx);
  Renderer();
  ~Renderer();
  //pipeline must be bound first, this should only be called by Shader::render
  void render(glm::dmat4 projection, GPU* gpu);
  bool exists() { return bool(obj.lock()); };
  void setOnceCallback(packDataCB);
  void setPerFrameCallback(packDataCB);
  void updateInstanceData(size_t resource, GPU* gpu);
  std::shared_ptr<Object> getObj() { return obj.lock(); };
  std::shared_ptr<Shader> getShader() { return shader; };
  std::shared_ptr<Mesh> getMesh() { return mesh; };
  RenderLayer* getLayer() { return layer; };
private:
  RenderLayer* layer;//no shared_ptr for globals
  std::shared_ptr<Shader> shader;
  std::shared_ptr<Mesh> mesh;
  std::weak_ptr<Object> obj;
  std::unique_ptr<GPUResource<Shader::Instance>> buffers;
  packDataCB packPreRender, packInitial;
};

