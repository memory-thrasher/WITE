#pragma once

namespace WITE {

  class export_def StaticMesh : public MeshSource {//for debug or simple things only, don't waste host ram on large ones
  public:
    StaticMesh(const Vertex* data, uint32_t size);
    inline StaticMesh() : StaticMesh(NULL, 0) {}
    uint32_t populateMeshCPU(Vertex* out, uint64_t maxVerts, const glm::dvec3* viewOrigin);
    BBox3D* getBbox(BBox3D* out);
    static void ImportObj(FILE* in, std::vector<Vertex>* out);
  private:
    BBox3D box;
    const void* data;
    uint32_t size;
  };

}
