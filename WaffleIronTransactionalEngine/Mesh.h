#pragma once

class Mesh : public WITE::Mesh {
public:
  Mesh(WITE::MeshSource* source);
  ~Mesh();
  uint32_t put(WITE::Vertex*, uint64_t offset, uint64_t maxSize, GPU*);
  static void proceduralMeshLoop(void* semaphore);//semaphore is type std::atomic<uint8_t>
private:
  static constexpr size_t VERTEX_BUFFERS = 3;//active read, standby, writing
  typedef struct {
    VkDeviceSize vbStart, vbLength;//uint64
  } subbuf_t;
  typedef struct VertexBuffer {
    BackedBuffer verts;
    size_t len;
    VertexBuffer(GPU*);
    ~VertexBuffer();
  } VertexBuffer;
  static std::unique_ptr<VertexBuffer[]> makeBuffersFor(GPU*);
  static WITE::GPUResource<VertexBuffer[]> vertexBuffers;
  static WITE::SyncLock allmeshes_lock;
  static AtomicLinkedList<Mesh> allMeshes;
  WITE::MeshSource* source;
  WITE::GPUResource<subbuf_t[]> subbuf;
  std::unique_ptr<subbuf_t[]> makeSubbuf(GPU*);
  AtomicLinkedList<Mesh> allMeshes_node;
  AtomicLinkedList<Renderer> owners;
  friend class Renderer;
};

