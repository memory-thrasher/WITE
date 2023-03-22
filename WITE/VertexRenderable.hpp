#pragma once

#include "Renderable.hpp"
#include "Buffer.hpp"
#include "VertexShader.hpp"

namespace WITE::GPU {

  //this is too a abstract. Where the vertexes come from is further abstracted., Note this is NOT to be used for mesh rendering.
  template<ShaderData D, VertexModel VM>//FIXME limitation: all vert buffers must share a vertex layout atm
  class VertexRenderable : public Renderable<D> {
  public:
    typedef Vertex<VM> vertex_t;
    constexpr static size_t vertBufferCount = D.count(ShaderData::Vertex);
  private:
    vk::DeviceSize offsets[vertBufferCount];
    // vk::DeviceSize sizes[vertBufferCount];
    // vk::DeviceSize strides[vertBufferCount];
    BufferBase* vertBuffers[vertBufferCount];
  protected:
    VertexRenderable(layer_t layer, VertexShader<D, VM>* s) :
      Renderable<D>(layer, s) {};
    //implementation should provide public setting mechanism (constructor or method or make-on-demand etc)
    void setBuffers(BufferBase* buffers[vertBufferCount]) {
      for(size_t i = 0;i < vertBufferCount;i++) {
	vertBuffers[i] = buffers[i];
	// offsets[i] = 0;
	// sizes[i] = buffers[i].getSize();
	// strides[i] = sizeof(vertex_t);
      }
    };
  public:
    virtual void preBindVertBuffers(WorkBatch cmd, size_t gpu) = 0;
    virtual size_t getVertexCount() = 0;
    void render(WorkBatch cmd) override {
      size_t gpu = cmd.getGpuIdx();
      preBindVertBuffers(cmd, gpu);
      cmd.bindAndDraw<vertBufferCount>(vertBuffers, getVertexCount(), 0, offsets);
    };
  };

};
