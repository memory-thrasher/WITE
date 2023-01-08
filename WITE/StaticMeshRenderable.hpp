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
    MeshBuffer meshBuffer;
    #error TODO make this static! ^ only need one buffer since they all have the same data!
  public:
    ShaderMeshRenderable(layer_t later, VertexShader<passShaderData(D), passVertexModel(VM)>* s) : UBER(layer, s)
    {
      MeshBuffer* b = &meshBuffer;
      setBuffers(&b);
    };
    void preBindVertBuffers(ElasticCommandBuffer& cmd, size_t gpu) override {
      meshBuffer.write(MESH.get(), vertCount);
    }
    size_t getVertexCount() { return vertCount; };
  };

};

