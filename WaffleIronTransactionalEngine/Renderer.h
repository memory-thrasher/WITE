#pragma once

#include "BackedBuffer.h"
#include "Shader.h"
#include "AtomicLinkedList.h"

class Object;
class RenderLayer;
class Mesh;

class Renderer {
public:
  typedefCB(packDataCB, void, Renderer*, std::shared_ptr<class WITE::ShaderResource>*, GPU*)//array of shared pointers, one per descriptor
  static void bind(Object* o, Shader* s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx);
  static void unbind(Object*, WITE::renderLayerIdx);
  Renderer();
  ~Renderer();
  //pipeline must be bound first, this should only be called by Shader::render
  void render(VkCommandBuffer cmd, glm::dmat4 projection, GPU* gpu);
  bool exists() { return bool(obj); };
  void setOnceCallback(packDataCB);
  void setPerFrameCallback(packDataCB);
  void updateInstanceData(size_t resource, GPU* gpu);
  Object* getObj() { return obj; };
  Shader* getShader() { return shader; };
  std::shared_ptr<Mesh> getMesh() { return mesh; };
  RenderLayer* getLayer() { return layer; };
private:
  RenderLayer* layer;//no shared_ptr for globals
  Shader* shader;
  std::shared_ptr<Mesh> mesh;
  Object* obj;
  std::unique_ptr<GPUResource<Shader::Instance>> buffers;
  AtomicLinkedList<Renderer> mesh_owners_node;
  packDataCB packPreRender, packInitial;
};

