#pragma once

#include "Object.h"

class Mesh
{
public:
  Mesh(std::shared_ptr<WITE::MeshSource> source);
  ~Mesh();
  virtual uint32_t put(void*, uint32_t offset, uint32_t maxSize, GPU*) = 0;
  static void proceduralMeshLoop(std::atomic_uint8_t* semaphore);
private:
  static constexpr size_t VERTEX_BUFFERS = 2;
  typedef struct {
    size_t vbStart, vbLength;
  } subbuf_t;
  typedef struct VertexBuffer {
    BackedBuffer verts;
    size_t len;
    ~VertexBuffer();
  } VertexBuffer;
  static std::unique_ptr<VertexBuffer[VERTEX_BUFFERS]> makeBuffersFor(GPU*);
  static GPUResource<VertexBuffer[VERTEX_BUFFERS]> vertexBuffers;
  static WITE::SyncLock allmeshes_lock;
  static AtomicLinkedList<Mesh> allMeshes;
  std::shared_ptr<WITE::MeshSource> source;
  GPUResource<subbuf_t[VERTEX_BUFFERS]> subbuf;
  AtomicLinkedList<Mesh> allMeshes_node;
  AtomicLinkedList<Renderer> owners;
  friend class Renderer;
};

