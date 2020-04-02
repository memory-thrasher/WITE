#include "stdafx.h"
#include "Renderer.h"


Renderer::Renderer() : obj(NULL) {}

Renderer::~Renderer() {}

//o must not be null. call unbind to disable rendering on this layer
void Renderer::bind(Object* o, Shader* s, VMesh* m, renderLayerIdx rlIdx) {
  ScopeLock slo(&o->lock);//each renderer belongs to exactly one object, which owns exactly MAX_RENDER_LAYER renderers. Save mutexes by using the owning object's lock.
  Renderer* r = o->renderLayer[rlIdx];
  Object* obj = r->obj;
  r->obj = o;
  if(!obj) {
    r->layer = &vkSingleton.renderLayers[rlIdx];
    ScopeLock sl(&r->layer->memberLock);
    r->layer->members.push_back(r);
  }
  if(!obj || m != r->mesh) {
    if (obj) {
      ScopeLock sl(&r->mesh->lock);
      vectorPurge(r->mesh->owners, r);
    }
    r->mesh = m;
    ScopeLock sl(&m->lock);
    m->owners.push_back(r);
  }
  if(!obj || s != r->shader) {
    if (obj) {
      ScopeLock sl(&r->shader->lock);
      vectorPurge(r->shader->renderers[layer->idx], r);
    }
    r->shader = s;
    ScopeLock sl(&s->lock);
    s->renderers.push_back(r);
  }
}

void Renderer::unbind(Object* o, renderLayerIdx rlIdx) {
  ScopeLock slo(&o->lock);//each renderer belongs to exactly one object, which owns exactly MAX_RENDER_LAYER renderers. Save mutexes by using the owning object's lock.
  Renderer* r = o->renderLayer[rlIdx];
  Object* obj = r->obj;
  if (!obj) return;
  if (obj != o) CRASH;
  r->obj = NULL;
  {
    ScopeLock sl(&r->layer->memberLock);
    vectorPurge(r->layer->members, r);
  }
  {
    ScopeLock sl(&r->mesh->lock);
    vectorPurge(r->mesh->owners, r);
  }
  {
    ScopeLock sl(&r->shader->lock);
    vectorPurge(r->shader->renderers[layer->idx], r);
  }
}

void Renderer::setOnceCallback(packDataCB d) {
  packInitial = d;
}

void Renderer::setPerFrameCallback(packDataCB d) {
  packPreRender = d;
}

void Renderer::render(glm::mat4d projection, GPU* gpu) {
  //pipeline is already selected
  Shader::Instance* buffer = buffers->get(gpu);
  if (!buffer->inited) {
    buffer->inited = true;
    if(packInitial) packInitial->call(this, buffer->resources.get(), gpu);
  }
  (glm::mat4)*buffer->resources[0]->map() = obj->getTrans()->project(projection);
  buffer->resources[0]->unmap();
  updateInstanceData(0, gpu);
  if(packPreRender) packPreRender->call(this, buffer->resources.get(), gpu);
  //bind descriptor set
  //bind vertex buffer
  //draw
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
