#include "stdafx.h"
#include "VMesh.h"
#include "VWindow.h"

VMesh::VMesh(MeshSource* source) : source(source) {}

VMesh::~VMesh() {}

uint32_t VMesh::put(void* out, uint32_t offset, uint32_t maxSize, GPU* gpu) {
	size_t camCount = 0, camIdx;//TODO better
	std::shared_ptr<Object> object, mostSignificantChild;
	VCamera* camera, *mostSignificantCamera;
	float relativeResolution, highestResolution = 0;
	BBox3D tempBox1, tempBox2;
	auto end = gpu->windows.end();
	for (auto window = gpu->windows.begin();window != end;window++) {
		camIdx = 0;
		for (auto cam = (*window)->iterateCameras(camCount);camIdx < camCount;cam++, camIdx++) {
			camera = cam;
			//TODO cull if camera does not render this layer
			for (auto obj = owners.nextLink();obj.get() != &owners;obj = obj->nextLink()) {
				//camera tangent and distance comparison vs best
				object = obj->getRef()->getObj();
				relativeResolution = camera->approxScreenArea(object->getTrans()->transform(
					source->getBbox(&tempBox1), &tempBox2));
				if (relativeResolution > highestResolution) {
					highestResolution = relativeResolution;
					mostSignificantCamera = camera;
					mostSignificantChild = object;
				}
			}
		}
	}
	if (!highestResolution) return 0;
	//TODO compute option; if gpu is underutilized [load params to static uniform buffer, run and wait, load vert count from unibuf ]
	//TODO transform camera location to local space of mesh
	return source->populateMeshCPU(static_cast<uint8_t*>(out) + offset * SIZEOF_VERTEX,
		glm::min(maxSize / SIZEOF_VERTEX, (uint32_t)highestResolution), mostSignificantCamera->getLocation());
}

void VMesh::proceduralMeshLoop(std::atomic_uint8_t* semaphore) {
	std::shared_ptr<AtomicLinkedList<VMesh>> next;
	std::shared_ptr<VMesh> mesh;
	size_t i = 0, gpuIdx;
	uint32_t tempOffset;
	unsigned char* map;
	while (true) {//FIXME death condition?
		for (gpuIdx = 0;gpuIdx < vkSingleton.gpuCount;gpuIdx++) {
			map = vertexBuffers[gpuIdx][i].map();
			vertexBufferLen[gpuIdx][i] = 0;
			next = allMeshes.nextLink();
			while (next.get() != &allMeshes) {
				mesh = next->getRef();
				if (!mesh) continue;
				tempOffset = vertexBufferLen[gpuIdx][i];
				tempOffset += mesh->put(static_cast<void*>(map), tempOffset, vkSingleton.vramGrabSize - tempOffset,
					&vkSingleton.gpus[gpuIdx]);
				vertexBufferLen[gpuIdx][i] = tempOffset;
				next = next->nextLink();
			}
			vertexBuffers[gpuIdx][i].unmap();
		}
		semaphore->store(i+1, std::memory_order_release);
		while (semaphore->load(std::memory_order_consume) != 0) sleep(1);
		if (++i >= VERTEX_BUFFERS) i = 0;
	}
}
