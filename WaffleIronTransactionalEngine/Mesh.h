#pragma once

#include "Object.h"

class Mesh
{
public:
  Mesh(std::shared_ptr<WITE::MeshSource> source);
  ~Mesh();
  virtual uint32_t put(void*, uint64_t offset, uint64_t maxSize, GPU*) = 0;
  static void proceduralMeshLoop(std::atomic_uint8_t* semaphore);
private:
  static constexpr size_t VERTEX_BUFFERS = 2;
  typedef struct {
    size_t vbStart, vbLength;
  } subbuf_t;
  typedef struct VertexBuffer {
    BackedBuffer verts;
    size_t len;
    VertexBuffer(GPU*);
    ~VertexBuffer();
  } VertexBuffer;
  typedef GPUResource<Mesh::VertexBuffer[]> buffers_t;
  static std::unique_ptr<VertexBuffer[]> makeBuffersFor(GPU*);
  static GPUResource<VertexBuffer[]> vertexBuffers;
  static WITE::SyncLock allmeshes_lock;
  static AtomicLinkedList<Mesh> allMeshes;
  std::shared_ptr<WITE::MeshSource> source;
  GPUResource<subbuf_t[]> subbuf;
  std::unique_ptr<subbuf_t[]> makeSubbuf(GPU*);
  AtomicLinkedList<Mesh> allMeshes_node;
  AtomicLinkedList<Renderer> owners;
  friend class Renderer;
};

