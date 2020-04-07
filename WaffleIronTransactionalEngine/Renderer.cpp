#include "stdafx.h"
#include "Renderer.h"
#include "RenderLayer.h"
#include "Object.h"
#include "Shader.h"
#include "Mesh.h"

Renderer::Renderer() : obj() {}

Renderer::~Renderer() {}

//o must not be null. call unbind to disable rendering on this layer
void Renderer::bind(std::shared_ptr<Object> o, std::shared_ptr<Shader> s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx) {
  WITE::ScopeLock slo(&o->lock);//each renderer belongs to exactly one object, which owns exactly MAX_RENDER_LAYER renderers. Save mutexes by using the owning object's lock.
  Renderer* r = &o->renderLayer[rlIdx];
  bool didExist = bool(r->obj.lock());
  if(!didExist) {
    r->obj = o;
    r->layer = &vkSingleton.renderLayers[rlIdx];
    WITE::ScopeLock sl(&r->layer->memberLock);
    r->layer->members.push_back(r);
  }
  if(!didExist || m != r->mesh) {
    if (didExist) {
      WITE::ScopeLock sl(&r->mesh->lock);
      vectorPurge(r->mesh->owners, r);//TODO atomic linked list
    }
    r->mesh = m;
    WITE::ScopeLock sl(&m->lock);
    m->owners.push_back(r);
  }
  if(!didExist || s != r->shader) {
    if (didExist) {
      WITE::ScopeLock sl(&r->shader->lock);
      vectorPurge(r->shader->renderers[layer->idx], r);
    }
    r->shader = s;
    WITE::ScopeLock sl(&s->lock);
    s->renderers.push_back(r);
  }
}

void Renderer::unbind(std::shared_ptr<Object> o, renderLayerIdx rlIdx) {
  WITE::ScopeLock slo(&o->lock);//each renderer belongs to exactly one object, which owns exactly MAX_RENDER_LAYER renderers. Save mutexes by using the owning object's lock.
  Renderer* r = o->renderLayer[rlIdx];
  bool didExist = r->obj.lock();
  if (!didExist) return;
  r->obj = std::weak_ptr<Object>();
  {
    WITE::ScopeLock sl(&r->layer->memberLock);
    vectorPurge(r->layer->members, r);
  }
  {
    WITE::ScopeLock sl(&r->mesh->lock);
    vectorPurge(r->mesh->owners, r);
  }
  {
    WITE::ScopeLock sl(&r->shader->lock);
    vectorPurge(r->shader->renderers[layer->idx], r);
  }
}

void Renderer::setOnceCallback(packDataCB d) {
  packInitial = d;
}

void Renderer::setPerFrameCallback(packDataCB d) {
  packPreRender = d;
}

void Renderer::render(glm::dmat4 projection, GPU* gpu) {
  //pipeline is already selected
  Shader::Instance* buffer = buffers->get(gpu)[vertBuffer];
  auto subbuf = mesh->subbuf.get(gpu);
  if (!buffer->inited) {
    buffer->inited = true;
    if(packInitial) packInitial->call(this, buffer->resources.get(), gpu);
  }
  (glm::mat4)*buffer->resources[0]->map() = obj->getTrans()->project(projection);
  buffer->resources[0]->unmap();
  updateInstanceData(0, gpu);
  if(packPreRender) packPreRender->call(this, buffer->resources.get(), gpu);
  vkCmdBindDescriptorSet(cmd, VK_PUPELINE_BIND_POINT_GRAPHICS, shader->resources.get(gpu)->pipelineLayout, 0, 1, &buffer->descSet, 0, NULL);
  vkCmdBindVertexBuffers(cmd, 0, 1, &Mesh::vertexBuffers.get(gpu)[vertBuffer]->verts, &subbuf.vbStart);//TODO move this to camera level, subdivide on draw
  vkCmdDraw(cmd, subbuf.vbLength, 1, 0, 0);
}

void updateInstanceData(size_t resource, GPU* gpu) {//TODO batchify?
  Shader::Instance* buffer = buffers->get(gpu);
  struct Shader::shaderGpuResources* shaderRes = shader->resources.get(gpu);
  VkWriteDescriptorSet write =
    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, buffer->descSet, resource, 0, 1, resourceLayout[resource].type, NULL, NULL, NULL };
  switch(write.descriptorType) {
  case SHADER_RESOURCE_SAMPLED_IMAGE:
    write.pImageInfo = &((BackedImage*)buffer->resources[resource].get())->info;
    break;
  case SHADER_RESOURCE_SAMPLED_IMAGE:
    write.pBufferInfo = ((BackedBuffer*)buffer->resources[resource].get())->info;
    break;
  }
  vkUpdateDescriptorSet(gpu->device, 1, &write, 0, NULL);
}


/*
  (glm::mat4)*uniformBuffer->map() = glm::mat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) *
  glm::perspective(glm::radians(45.0f) * size.height / size.width, (float)size.width / size.height,
  0.1f, 100.0f) *
  glm::lookAt(glm::vec3(-5, 3, -10), glm::vec3(0, 0, 0), glm::vec3(0, -1, 0)) * glm::mat4();//ident
  uniformBuffer->unmap();
*/
