#pragma once

#include "Renderable.hpp"
#include "Buffer.hpp"

namespace WITE::GPU {

  //this is too a abstract. Where the vertexes come from is further abstracted., Note this is NOT to be used for mesh rendering.
  template<acceptShaderData(D), acceptVertexModel(VM)>//FIXME limitation: all vert buffers must share a vertex layout atm
  class VertexRenderable : public Renderable<passShaderData(D)> {
  public:
    typedef Vertex<passVertexModel(VM)> vertex_t;
    constexpr static size_t vertBufferCount = parseShaderData(D).count(ShaderData::Vertex);
  private:
    vk::DeviceSize offsets[vertBufferCount];
    // vk::DeviceSize sizes[vertBufferCount];
    // vk::DeviceSize strides[vertBufferCount];
    BufferBase* vertBuffers[vertBufferCount];
  protected:
    VertexRenderable(layer_t later, VertexShader<passShaderData(D), passVertexModel(VM)>* s) :
      Renderable<passShaderData(D)>(layer, s) {};
    //implementation should provide public setting mechanism (constructor or method or make-on-demand etc)
    void setBuffers(BufferBase* buffers[vertBufferCount]) {
      for(size_t i = 0;i < vertBufferCount;i++) {
	vertBuffers[i] = buffers[i];
	// offsets[i] = 0;
	// sizes[i] = buffers[i].getSize();
	strides[i] = sizeof(vertex_t);
      }
    };
  public:
    virtual void preBindVertBuffers(ElasticCommandBuffer& cmd, size_t gpu) = 0;
    void render(ElasticCommandBuffer& cmd) override {
      size_t gpu = cmd->getGpu()->getIndex();
      if(preBindVertBuffers)
	preBindVertBuffers(cmd, gpu);
      vk::Buffer buffers[vertBufferCount];
      for(size_t i = 0;i < vertBufferCount;i++)
	buffers[i] = vertBuffers[i].get(gpu);
      cmd->bindVertexBuffers(0, vertBufferCountbuffers, offsets);
      bindVertexBuffers(cmd, gpu);
      bind(gpu, cmd);
      cmd->draw(getVertCount(), 1, 0, 0);//FIXME "instances" of what?
    };
    virtual size_t getVertexCount() = 0;
  };

};
