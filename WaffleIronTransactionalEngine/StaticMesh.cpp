#include "Internal.h"

namespace WITE {

  StaticMesh::StaticMesh(const Vertex* data, uint32_t size) {
    this->data = data;
    this->size = size,
    this->box = BBox3D(mangle<Mangle_ComponentwiseMin<3, Vertex>, Vertex>(data, size).pos,
      mangle<Mangle_ComponentwiseMax<3, Vertex>, Vertex>(data, size).pos);
  };

  uint32_t StaticMesh::populateMeshCPU(Vertex* out, uint64_t maxVerts, const glm::dvec3* viewOrigin) {
    if(size <= maxVerts) {
      memcpy(out, data, size * SIZEOF_VERTEX);
      return size;
    }
    return 0;
  };

  BBox3D* StaticMesh::getBbox(BBox3D* out) {
    return &box;
  };

}
