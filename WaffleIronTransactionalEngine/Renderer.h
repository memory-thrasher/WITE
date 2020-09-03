#pragma once

#include "BackedBuffer.h"
#include "Shader.h"
#include "AtomicLinkedList.h"

class Object;
class RenderLayer;
class Mesh;

class Renderer : public WITE::Renderer {
public:
  static void bind(Object* o, Shader* s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx);
  static void unbind(Object*, WITE::renderLayerIdx);
  Renderer();
  ~Renderer();
  //pipeline must be bound first, this should only be called by Shader::render
  void render(VkCommandBuffer cmd, glm::dmat4 projection, GPU* gpu);
  void setOnceCallback(packDataCB);
  void setPerFrameCallback(packDataCB);
  void updateInstanceData(size_t resource, WITE::GPU* gpu);
  WITE::Object* getObj();
  WITE::Shader* getShader();
  std::shared_ptr<WITE::Mesh> getMesh();
  Object* getObj_intern();
  Shader* getShader_intern();
  std::shared_ptr<Mesh> getMesh_intern();
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

