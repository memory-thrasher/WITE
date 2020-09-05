#include "stdafx.h"
#include "Renderer.h"
#include "RenderLayer.h"
#include "Object.h"
#include "Shader.h"
#include "Mesh.h"
#include "BackedImage.h"
#include "constants.h"
#include "Debugger.h"

Renderer::Renderer() : obj(), packPreRender(NULL), packInitial(NULL), mesh_owners_node(this) {}

Renderer::~Renderer() {}

//o must not be null. call unbind to disable rendering on this layer
void WITE::Renderer::bind(WITE::Object* o, WITE::Shader* s, std::shared_ptr<WITE::Mesh> m, WITE::renderLayerIdx rlIdx) {
  ::Renderer::bind(static_cast<::Object*>(o), static_cast<::Shader*>(s), std::static_pointer_cast<::Mesh>(m), rlIdx);
}

void Renderer::bind(Object* o, Shader* s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx) {
  WITE::ScopeLock slo(&o->lock);//each renderer belongs to exactly one object. Save mutexes by using the owning object's lock.
  Renderer* r = &o->renderLayer[rlIdx];
  bool didExist = bool(r->obj);
  if(!didExist) {
    r->obj = o;
    r->layer = &vkSingleton.renderLayers[rlIdx];
    WITE::ScopeLock sl(&r->layer->memberLock);
    r->layer->members.push_back(r);
  }
  if(!didExist || m != r->mesh) {
    if (didExist)
      r->mesh_owners_node.drop();
    r->mesh = m;
    m->owners.append(&r->mesh_owners_node);
  }
  if(!didExist || s != r->shader) {
    if (didExist) {
      WITE::ScopeLock sl(&r->shader->lock);
      WITE::vectorPurge(&r->shader->renderers[rlIdx], r);//TODO atomiclinkedlists for these too
    }
    r->shader = s;
    WITE::ScopeLock sl(&s->lock);
    s->renderers[rlIdx].push_back(r);
    r->buffers = std::make_unique<GPUResource<Shader::Instance>>
      (WITE::CallbackFactory<std::unique_ptr<Shader::Instance>, GPU*>::make<Shader>(s, &Shader::makeResources));
  }
}

void WITE::Renderer::unbind(Object* o, WITE::renderLayerIdx rlIdx) {
  ::Renderer::unbind(static_cast<::Object*>(o), rlIdx);
}

void Renderer::unbind(Object* o, WITE::renderLayerIdx rlIdx) {
  WITE::ScopeLock slo(&o->lock);//each renderer belongs to exactly one object, which owns exactly MAX_RENDER_LAYER renderers. Save mutexes by using the owning object's lock.
  Renderer* r = &o->renderLayer[rlIdx];
  bool didExist = bool(r->obj);
  if (!didExist) return;
  r->obj = NULL;
  {
    WITE::ScopeLock sl(&r->layer->memberLock);
    WITE::vectorPurge(&r->layer->members, r);
  }
  r->mesh_owners_node.drop();
  {
    WITE::ScopeLock sl(&r->shader->lock);
    WITE::vectorPurge(&r->shader->renderers[rlIdx], r);
    r->buffers.release();
  }
}

void Renderer::setOnceCallback(packDataCB d) {
  packInitial = d;
}

void Renderer::setPerFrameCallback(packDataCB d) {
  packPreRender = d;
}

void Renderer::render(VkCommandBuffer cmd, glm::dmat4 projection, GPU* gpu) {
  //pipeline is already selected
  uint8_t vertBuffer = getVertBuffer();
  Shader::Instance* buffer = buffers->get(gpu);
  auto subbuf = mesh->subbuf.get(gpu, vertBuffer);//copy
  if(!subbuf.vbLength) return;
  if (!buffer->inited) {
    buffer->inited = true;
    if(packInitial) packInitial->call(this, buffer->resources.get(), gpu);
  }
  auto obj_trans = obj->getTrans();
  auto right = obj_trans.getMat();
  auto mvp = obj_trans.project(projection);
  //LOGMAT(projection, "left");
  //LOGMAT(right, "right");
  //LOGMAT(mvp, "mvp");
  //LOG("\nAddress of resource: %p\naddress of resources: %p\naddress of buffer: %p\n", (void*)buffer->resources.get(), (void*)&buffer->resources, (void*)buffer);//(outdated)first call: 0000000006905E78, second call: 00000000FDFDFDFD (crashed on next line)
  glm::mat4* transOut = reinterpret_cast<glm::mat4*>(buffer->resources[0]->map());
  transOut[0] = mvp;
  mvp[3] = glm::vec4();
  transOut[1] = mvp;//direction only without offset, for normals
  buffer->resources[0]->unmap();
  //LOG("Resource updated successfully\n");
  updateInstanceData(0, gpu);
  if(packPreRender) packPreRender->call(this, buffer->resources.get(), gpu);
  const char* objName = obj->getName();
  if(objName) Debugger::beginLabel(cmd, objName);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->resources.get(gpu)->pipelineLayout, 0, 1, &buffer->descSet, 0, NULL);
  //TODO vkCmdPushDescriptorSet? Test ordering of bind just before draw, without any transitions?
  vkCmdBindVertexBuffers(cmd, 0, 1, &Mesh::vertexBuffers.get(gpu, vertBuffer).verts.buffer, &subbuf.vbStart);
  //TODO move bind to camera level, subdivide on draw
  vkCmdDraw(cmd, subbuf.vbLength, 1, 0, 0);
  if(objName) Debugger::endLabel(cmd);
  //TODO test that cmd writes are still good here. vkCmdFillBuffer?
}

void Renderer::updateInstanceData(size_t resource, WITE::GPU* gpuE) {//TODO batchify?
  ::GPU* gpu = (::GPU*)gpuE;
  Shader::Instance* buffer = buffers->get(gpu);
  VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, buffer->descSet, (uint32_t)resource, 0,
				 1, (VkDescriptorType)shader->resourceLayout[resource].type, NULL, NULL, NULL };
  switch(write.descriptorType) {
  case SHADER_RESOURCE_SAMPLED_IMAGE:
    write.pImageInfo = &((BackedImage*)buffer->resources[resource].get())->info;
    //TODO sampler type in write?
    break;
  case SHADER_RESOURCE_UNIFORM:
    write.pBufferInfo = &((BackedBuffer*)buffer->resources[resource].get())->info;
    break;
  default:
    CRASH("Unsupported/unrecognized shader resource type: %d\n", write.descriptorType);
    break;
  }
  vkUpdateDescriptorSets(gpu->device, 1, &write, 0, NULL);
}

WITE::Object* Renderer::getObj() {
  return obj;
}

WITE::Shader* Renderer::getShader() {
  return shader;
}

std::shared_ptr<WITE::Mesh> Renderer::getMesh() {
  return mesh;
}

Object* Renderer::getObj_intern() {
  return obj;
}

Shader* Renderer::getShader_intern() {
  return shader;
}

std::shared_ptr<Mesh> Renderer::getMesh_intern() {
  return mesh;
}
