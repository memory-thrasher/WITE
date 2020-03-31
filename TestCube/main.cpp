#include "../WaffleIronTransactionalEngine/Export.h"

static WITE::Database mainmenu(NULL, 65536);

static class Cube {
  struct savedData {
    glm::dmat4x4 transform;
  };
  auto shader = WITE::Shader::make();//TODO
  class CubeMesh : WITE::MeshSource {
    
    uint32_t populateMeshCPU(void* out, uint32_t maxVerts, glm::vec3* viewOrigin_unused) {
    };
    BBox3D* getBbox(BBox3D* out) { return &box; };
  };
  auto mesh = WITE::Mesh::make(std::make_shared<WITE::MeshSource>(CubeMesh));//TODO
  void update(WITE::Database::Entry e) {
    //TODO
  };
  void init(WITE::Database::Entry e) {
    //TODO export interface
    auto obj = WITE::Object::make();
    WITE::Renderer::bind(obj, shader, mesh, 0);
  };
  void destroy(WITE::Database::Entry e) {
  };
  WITE::Database::type type = 1;
  WITE::Database::typeHandles = { &update, &init, &destroy };
}

void registerTypes() {
  WITE::Database::registerType(Cube::type, Cube::typeHandles);
}

int main(int argc, char** argv) {
  WITE::WITE_INIT();
  auto window = WITE::Window::make();
  registerTypes();
  WITE::create(Cube::type);//TODO
  //TODO make object
  WITE::enterMainLoop(&mainmenu);
}
