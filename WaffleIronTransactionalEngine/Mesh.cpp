#include "Internal.h"

decltype(Mesh::vertexBuffers) Mesh::vertexBuffers(decltype(Mesh::vertexBuffers)::constructor_F::make(&Mesh::makeBuffersFor));
AtomicLinkedList<Mesh> Mesh::allMeshes;

Mesh::Mesh(WITE::MeshSource* source) : source(source),
  subbuf(decltype(subbuf)::constructor_F::make<Mesh>(this, &Mesh::makeSubbuf)),
  allMeshes_node(this) {
  allMeshes.append(&allMeshes_node);
}

Mesh::~Mesh() {
  allMeshes_node.drop();
}

std::unique_ptr<Mesh::VertexBuffer[]> Mesh::makeBuffersFor(GPU* gpu) {
  VertexBuffer* ret = reinterpret_cast<VertexBuffer*>(malloc(sizeof(VertexBuffer)*VERTEX_BUFFERS));
  for(size_t i = 0;i < VERTEX_BUFFERS;i++)
    new(&ret[i])VertexBuffer(gpu);
  return std::unique_ptr<VertexBuffer[]>(ret);
}

Mesh::VertexBuffer::VertexBuffer(GPU* gpu) : len(0),
  verts(gpu, vkSingleton.vramGrabSize/VERTEX_BUFFERS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {};

Mesh::VertexBuffer::~VertexBuffer() = default;

std::unique_ptr<Mesh::subbuf_t[]> Mesh::makeSubbuf(GPU* g) {
  return std::make_unique<subbuf_t[]>(Mesh::VERTEX_BUFFERS);
}

uint32_t Mesh::put(WITE::Vertex* out, uint64_t offset, uint64_t maxSize, GPU* gpu) {
  size_t camCount = 0, camIdx;//TODO better
  Object* object, *mostSignificantChild;
  WITE::Camera* cam, *mostSignificantCamera;
  float relativeResolution, highestResolution = 0;
  WITE::BBox3D tempBox1, tempBox2;
  auto end = gpu->windows.end();
  for (auto window = gpu->windows.begin();window != end;window++) {
    camCount = (*window)->getCameraCount();
    if(!camCount) continue;
    for (camIdx = 0;camIdx < camCount;++camIdx) {
      cam = (*window)->getCamera(camIdx);
      //TODO cull if camera does not render this layer
    startovermesh:
      for (auto obj = owners.nextLink();obj != &owners;obj = obj->nextLink()) {
	if(!obj->linked()) goto startovermesh;
	//camera tangent and distance comparison vs best
	object = obj->getRef()->getObj_intern();
        if(!object->isInitialized()) continue;
	relativeResolution = database->getCurrentFrame() == 0 ? 100 : cam->approxScreenArea(object->getTrans().transform(source->getBbox(&tempBox1), &tempBox2));
	if (relativeResolution > highestResolution) {
	  highestResolution = relativeResolution;
	  mostSignificantCamera = cam;
	  mostSignificantChild = object;
	}
      }
    }
  }
  if (!highestResolution) return 0;
  //TODO compute option; if gpu is underutilized [load params to static uniform buffer, run and wait, load vert count from unibuf ]
  glm::dvec3 camLoc = mostSignificantCamera->getLocation();
  //TODO transform camera location to local space of mesh
  return source->populateMeshCPU(out + offset, glm::min<uint64_t>(maxSize / SIZEOF_VERTEX, (uint64_t)highestResolution), &camLoc);
}

void Mesh::proceduralMeshLoop(void* semRaw) {
  std::atomic<int8_t>* semaphore = (std::atomic<int8_t>*)semRaw;
  AtomicLinkedList<Mesh>* next;
  Mesh* mesh;
  size_t i = 0, gpuIdx, len;
  uint32_t tempOffset;
  int8_t semaphoreRead;
  void* map;
  while (true) {
    for (gpuIdx = 0;gpuIdx < vkSingleton.gpuCount;gpuIdx++) {
      auto vb = vertexBuffers.getPtr(gpuIdx, i);
      map = vb->verts.map();
      vb->len = 0;
      next = allMeshes.nextLink();
      while (next != &allMeshes) {
	mesh = next->getRef();
	if (!mesh) continue;
        auto subbuf = mesh->subbuf.getPtr(gpuIdx, i);
	tempOffset = vb->len;
        len = mesh->put(static_cast<WITE::Vertex*>(map), tempOffset, vkSingleton.vramGrabSize - tempOffset, vkSingleton.gpus[gpuIdx]);
        *subbuf = { tempOffset, len };
	tempOffset += len;
	vb->len = tempOffset;
	next = next->nextLink();
      }
      vb->verts.unmap();
    }
    semaphore->store(i+1, std::memory_order_release);
    do {
      WITE::sleep(1);
      semaphoreRead = semaphore->load(std::memory_order_consume);
      if(semaphoreRead < 0) goto exit;
    } while (semaphoreRead != 0);
    if (++i >= VERTEX_BUFFERS) i = 0;
  }
exit:
  semaphore->store(-2, std::memory_order_release);
}

std::shared_ptr<WITE::Mesh> WITE::Mesh::make(WITE::MeshSource* source) {
  return std::make_shared<::Mesh>(source);
}
