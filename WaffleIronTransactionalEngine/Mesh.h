#pragma once

#include "Object.h"

class Mesh
{
public:
  Mesh(std::shared_ptr<MeshSource> source);
  ~Mesh();
  virtual uint32_t put(void*, uint32_t offset, uint32_t maxSize, GPU*) = 0;
  static void proceduralMeshLoop(std::atomic_uint8_t* semaphore);
private:
  constexpr const size_t VERTEX_BUFFERS = 2;
  typedef struct {
    size_t vbStart, vbLength;
  } subbuf_t;
  typedef struct VertexBuffer {
    BackedBuffer verts;
    size_t len;
    ~VertexBuffer();
  };
  static std::unique_ptr<VertexBuffer[VERTEX_BUFFERS]> makeBuffersFor(GPU*);
  static GPUResource<VertexBuffer[VERTEX_BUFFERS]> vertexBuffers;
  static SyncLock allmeshes_lock;
  static AtomicLinkedList<Mesh> allMeshes;
  std::shared_ptr<MeshSource> source;
  GPUResource<subbuf_t[VERTEX_BUFFERS]> subbuf;
  AtomicLinkedList<Mesh> allMeshes_node;
  AtomicLinkedList<Renderer> owners;
  friend class Renderer;
};

class StaticMesh : public MeshSource {//for debug or simple things only, don't waste host ram on large ones
public:
  StaticMesh(BBox3D box, void* data, uint32_t size) : box(box), data(data), size(size) {}
  uint32_t populateMeshCPU(void* out, uint32_t maxVerts, glm::vec3* viewOrigin) {
    if (size <= maxVerts) {
      memcpy(out, data, size * FLOAT_BYTES);
      return size;
    }
    return 0;
  };
  BBox3D* getBbox(BBox3D* out) { return &box; };
  //return newOrHere(out)BBox3D(box); };
private:
  BBox3D box;
  void* data;
  uint32_t size;
};
