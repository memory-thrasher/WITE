#pragma once

namespace WITE {

  union Vertex {
    static constexpr size_t FLOATS_PER_VERT = 9;
    float floats[1];
    struct {
      glm::vec3 pos, norm, data;
    };
    struct {
      float x, y, z, nx, ny, nz, r, g, b;
    };
    Vertex(float x, float y, float z, float nx, float ny, float nz, float r, float g, float b) : pos(x, y, z), norm(nx, ny, nz), data(r, g, b) {}
    Vertex() : pos(), norm(), data() {}
    Vertex(glm::vec3 p, glm::vec3 n, glm::vec3 d) : pos(p), norm(n), data(d) {}
    Vertex(glm::vec3 p, glm::vec3 n) : pos(p), norm(n), data() {}
    Vertex(glm::vec3 p) : pos(p), norm(), data() {}
    inline operator void*() {//for memcpy
      return static_cast<void*>(this);
    };
    inline float& operator[](size_t idx) {
      return floats[idx];
    };
    inline const float& operator[](size_t idx) const {
      return floats[idx];
    };
  };

  class export_def MeshSource {//mesh source can redirect to file load or cpu procedure, but should not store any mesh data. Mesh does that, per LOD.
  public:
    virtual uint32_t populateMeshCPU(Vertex*, uint64_t maxVerts, const glm::dvec3* viewOrigin) = 0;//returns number of verts used
    virtual Shader* getComputeMesh(void) { return NULL; };
    virtual BBox3D* getBbox(BBox3D* out = NULL) = 0;//not transformed
    virtual ~MeshSource() = default;
  };

};
