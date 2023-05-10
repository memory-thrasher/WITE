#pragma once

#include "VertexRenderable.hpp"

namespace WITE::GPU {

  struct staticMeshCache {
    static std::map<void*, staticMeshCache> cache;
    static Util::SyncLock bufferMutex;
    std::unique_ptr<BufferBase> meshBuffer;
    deviceMask_t bufferInitialized;
  };

  template<ShaderData D, VertexModel VM, logicalDeviceMask_t LDM, Collections::LiteralList<Vertex<VM>> MESH>
  class StaticMeshRenderable : public VertexRenderable<D, VM> {
  public:
    typedef VertexShader<D, VM> shader_t;
  private:
    typedef VertexRenderable<D, VM> UBER;
    static_assert(UBER::vertBufferCount == 1);
    static constexpr usage_t USAGE = GpuResource::USAGE_VERTEX | GpuResource::USAGE_HOST_WRITE | GpuResource::USAGE_GRAPHICS;
    //if we need to stage the data in a host-visible buffer first before putting it into the vertex buffer, that should be handled in Buffer
    static constexpr size_t vertSize = sizeof(typename shader_t::vertex_t);
    typedef Buffer<BufferSlotData(LDM, USAGE, MESH.len * vertSize)> MeshBuffer;
    deviceMask_t bufferInitialized;
  public:
    StaticMeshRenderable(layer_t layer, shader_t* s) : UBER(layer, s)
    {
      Util::ScopeLock lock(&staticMeshCache::bufferMutex);
      staticMeshCache& cache = staticMeshCache::cache[(void*)MESH.data];
      BufferBase* b;
      if(!cache.meshBuffer)
	[[unlikely]]
	cache.meshBuffer.reset(b = new MeshBuffer());
      else
	b = cache.meshBuffer.get();
      bufferInitialized = cache.bufferInitialized;
      lock.release();
      this->setBuffers(&b);
    };
    void preBindVertBuffers(WorkBatch cmd, size_t gpu) override {
      deviceMask_t flag = 1 << gpu;
      if(!(bufferInitialized & flag)) {
	Util::ScopeLock lock(&staticMeshCache::bufferMutex);
	staticMeshCache& cache = staticMeshCache::cache[(void*)MESH.data];
	if(!(cache.bufferInitialized & flag)) {
	  bufferInitialized = cache.bufferInitialized |= flag;
	  cache.meshBuffer->write(MESH.data, MESH.len, gpu);
	}
      }
    }
    virtual size_t getVertexCount() override { return MESH.len; };
  };

};

