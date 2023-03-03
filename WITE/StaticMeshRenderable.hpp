#pragma once

#include "VertexRenderable.hpp"

namespace WITE::GPU {

  template<acceptShaderData(D), acceptVertexModel(VM), logicalDeviceMask_t LDM,
	   size_t vertCount, std::array<vertCount, Vertex<passVertexModel(VM)>> MESH>
  class StaticMeshRenderable : public VertexRenderable<passShaderData(D), passVertexModel(VM)> {
  private:
    static_assert(vertBufferCount == 1);
    typedef VertexRenderable<passShaderData(D), passVertexModel(VM)> UBER;
    static constexpr usage_t USAGE = GpuResource::USAGE_VERTEX | GpuResource::USAGE_HOST_WRITE | GpuResource::USAGE_GRAPHICS;
    //if we need to stage the data in a host-visible buffer first before putting it into the vertex buffer, that should be handled in Buffer
    static constexpr size_t vertSize = sizeof(vertex_t);
    typedef Buffer<{LDM, USAGE, vertCount * vertSize}> MeshBuffer;
    static MeshBuffer meshBuffer;
    static Util::SyncLock bufferMutex;
    static deviceMask_t bufferInitialized = 0;
  public:
    ShaderMeshRenderable(layer_t layer, VertexShader<passShaderData(D), passVertexModel(VM)>* s) : UBER(layer, s)
    {
      MeshBuffer* b = &meshBuffer;
      setBuffers(&b);
    };
    void preBindVertBuffers(WorkBatch cmd, size_t gpu) override {
      Util::ScopeLock(&bufferMutex);
      deviceMask_t flag = 1 << gpu;
      if(!(bufferInitialized & flag)) {
	bufferInitialized |= flag;
	meshBuffer.write(MESH.get(), vertCount, gpu);
      }
    }
    size_t getVertexCount() { return vertCount; };
  };

};

