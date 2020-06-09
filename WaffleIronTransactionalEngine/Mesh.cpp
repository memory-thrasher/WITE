#include "stdafx.h"
#include "Mesh.h"
#include "Window.h"

decltype(Mesh::vertexBuffers) Mesh::vertexBuffers(decltype(Mesh::vertexBuffers)::constructor_F::make(&Mesh::makeBuffersFor));

Mesh::Mesh(std::shared_ptr<WITE::MeshSource> source) : source(source),
  subbuf(decltype(subbuf)::constructor_F::make<Mesh>(this, &Mesh::makeSubbuf)),
  allMeshes_node(std::shared_ptr<Mesh>(this)) {}

Mesh::~Mesh() {}

std::unique_ptr<Mesh::VertexBuffer[]> Mesh::makeBuffersFor(GPU* gpu) {
  VertexBuffer* ret = reinterpret_cast<VertexBuffer*>(malloc(sizeof(VertexBuffer)*VERTEX_BUFFERS));
  for(size_t i = 0;i < VERTEX_BUFFERS;i++)
    new(&ret[i])VertexBuffer(gpu);
  return std::unique_ptr<VertexBuffer[]>(ret);
}

Mesh::VertexBuffer::VertexBuffer(GPU* gpu) : verts(gpu, vkSingleton.vramGrabSize/VERTEX_BUFFERS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {};

std::unique_ptr<Mesh::subbuf_t[]> Mesh::makeSubbuf(GPU* g) {
  return std::make_unique<subbuf_t[]>(Mesh::VERTEX_BUFFERS);
}

uint32_t Mesh::put(void* out, uint64_t offset, uint64_t maxSize, GPU* gpu) {
  size_t camCount = 0, camIdx;//TODO better
  std::shared_ptr<Object> object, mostSignificantChild;
  WITE::Camera* cam, *mostSignificantCamera;
  float relativeResolution, highestResolution = 0;
  WITE::BBox3D tempBox1, tempBox2;
  auto end = gpu->windows.end();
  for (auto window = gpu->windows.begin();window != end;window++) {
    for (camCount = (*window)->getCameraCount(), cam = (*window)->getCamera(camIdx = 0);camIdx < camCount;cam = (*window)->getCamera(++camIdx)) {
      //TODO cull if camera does not render this layer
    startovermesh:
      for (auto obj = owners.nextLink();obj != &owners;obj = obj->nextLink()) {
	if(!obj->linked()) goto startovermesh;
	//camera tangent and distance comparison vs best
	object = obj->getRef()->getObj();
	relativeResolution = cam->approxScreenArea(object->getTrans().transform(source->getBbox(&tempBox1), &tempBox2));
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
  return source->populateMeshCPU(static_cast<uint8_t*>(out) + offset * SIZEOF_VERTEX,
				 glm::min<uint64_t>(maxSize / SIZEOF_VERTEX, (uint64_t)highestResolution), &camLoc);
}

void Mesh::proceduralMeshLoop(void* semRaw) {
  std::atomic<uint8_t>* semaphore = (std::atomic<uint8_t>*)semRaw;
  AtomicLinkedList<Mesh>* next;
  std::shared_ptr<Mesh> mesh;
  size_t i = 0, gpuIdx;
  uint32_t tempOffset;
  unsigned char* map;
  while (true) {//FIXME death condition?
    for (gpuIdx = 0;gpuIdx < vkSingleton.gpuCount;gpuIdx++) {
      map = vertexBuffers[gpuIdx][i].verts.map();
      vertexBuffers[gpuIdx][i].len = 0;
      next = allMeshes.nextLink();
      while (next != &allMeshes) {
	mesh = next->getRef();
	if (!mesh) continue;
	tempOffset = vertexBuffers[gpuIdx][i].len;
	tempOffset += mesh->put(static_cast<void*>(map), tempOffset, vkSingleton.vramGrabSize - tempOffset, vkSingleton.gpus[gpuIdx]);
	vertexBuffers[gpuIdx][i].len = tempOffset;
	next = next->nextLink();
      }
      vertexBuffers[gpuIdx][i].verts.unmap();
    }
    semaphore->store(i+1, std::memory_order_release);
    while (semaphore->load(std::memory_order_consume) != 0) WITE::sleep(1);
    if (++i >= VERTEX_BUFFERS) i = 0;
  }
}
