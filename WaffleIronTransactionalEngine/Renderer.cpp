#include "stdafx.h"
#include "Renderer.h"
#include "RenderLayer.h"
#include "Object.h"
#include "Shader.h"
#include "Mesh.h"
#include "BackedImage.h"
#include "constants.h"

Renderer::Renderer() : obj() {}

Renderer::~Renderer() {}

//o must not be null. call unbind to disable rendering on this layer
void Renderer::bind(std::shared_ptr<Object> o, std::shared_ptr<Shader> s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx) {
  WITE::ScopeLock slo(&o->lock);//each renderer belongs to exactly one object. Save mutexes by using the owning object's lock.
  Renderer* r = &o->renderLayer[rlIdx];
  bool didExist = bool(r->obj.lock());
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
  }
}

void Renderer::unbind(std::shared_ptr<Object> o, WITE::renderLayerIdx rlIdx) {
  WITE::ScopeLock slo(&o->lock);//each renderer belongs to exactly one object, which owns exactly MAX_RENDER_LAYER renderers. Save mutexes by using the owning object's lock.
  Renderer* r = &o->renderLayer[rlIdx];
  bool didExist = bool(r->obj.lock());
  if (!didExist) return;
  r->obj = std::weak_ptr<Object>();
  {
    WITE::ScopeLock sl(&r->layer->memberLock);
    WITE::vectorPurge(&r->layer->members, r);
  }
  r->mesh_owners_node.drop();
  {
    WITE::ScopeLock sl(&r->shader->lock);
    WITE::vectorPurge(&r->shader->renderers[rlIdx], r);
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
  Shader::Instance* buffer = &buffers->get(gpu)[vertBuffer];
  auto subbuf = mesh->subbuf.get(gpu, vertBuffer);//copy
  if (!buffer->inited) {
    buffer->inited = true;
    if(packInitial) packInitial->call(this, buffer->resources.get(), gpu);
  }
  *reinterpret_cast<glm::dmat4*>(buffer->resources[0]->map()) = obj.lock()->getTrans().project(projection);
  buffer->resources[0]->unmap();
  updateInstanceData(0, gpu);
  if(packPreRender) packPreRender->call(this, buffer->resources.get(), gpu);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->resources.get(gpu)->pipelineLayout, 0, 1, &buffer->descSet, 0, NULL);
  vkCmdBindVertexBuffers(cmd, 0, 1, &Mesh::vertexBuffers.get(gpu, vertBuffer).verts.buffer, &subbuf.vbStart);
  //TODO move bind to camera level, subdivide on draw
  vkCmdDraw(cmd, subbuf.vbLength, 1, 0, 0);
}

void Renderer::updateInstanceData(size_t resource, GPU* gpu) {//TODO batchify?
  Shader::Instance* buffer = &buffers->get(gpu)[vertBuffer];
  VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, buffer->descSet, (uint32_t)resource, 0,
				 1, (VkDescriptorType)shader->resourceLayout[resource].type, NULL, NULL, NULL };
  switch(write.descriptorType) {
  case SHADER_RESOURCE_SAMPLED_IMAGE:
    write.pImageInfo = &((BackedImage*)buffer->resources[resource].get())->info;
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

