#include <glm/glm.hpp>

#include "../WaffleIronTransactionalEngine/Export.h"
#include "cube_data.h"

struct cube {
  constexpr static WITE::Database::type type = 12;
  static WITE::StaticMesh mesh;
  WITE::Transform trans;
};

#define cubeMesh g_vb_solid_face_colors_Data
WITE::StaticMesh cube::mesh(WITE::BBox3D(), (uint8_t*)cubeMesh, sizeof(cubeMesh)/sizeof(cubeMesh[0]));

typedef struct {
  WITE::Shader* flat;
} shaders_t;

static shaders_t shaders;

static uint64_t frameIdx = 0;

void cubeUpdate(WITE::Database::Entry e) {
  frameIdx++;
  if(frameIdx % 1000 == 0) {
    LOG("living time: %llu\n", WITE::Time::frame());
  }
  if(WITE::Time::launchTime() + 10000000000 <= WITE::Time::frame()) {
    //if(frameIdx > 1000) {
    LOG("Delta nano: %llu\nFPS: %llu\nlife: %llu\n", WITE::Time::delta(), frameIdx/10, WITE::Time::frame() - WITE::Time::launchTime());
    WITE::gracefulExit();
  }
}

void cubeInit(WITE::Database::Entry e, WITE::Database::Entry* map) {
  auto o = (*database->getObjectInstanceFor(e)) = WITE::Object::make(e, offsetof(struct cube, trans), map);
  WITE::Renderer::bind(o, shaders.flat, WITE::Mesh::make(&cube::mesh), 0);
  WITE::Transform initialTrans(glm::dmat4(1));
  o->pushTrans(&initialTrans);
}

void cubeDestroy(WITE::Database::Entry e) {
  delete (*database->getObjectInstanceFor(e));
  (*database->getObjectInstanceFor(e)) = NULL;
}

const inline static WITE::Database::typeHandles cube_functions = {
  WITE::CallbackFactory<void, WITE::Database::Entry>::make(&cubeUpdate),
  WITE::CallbackFactory<void, WITE::Database::Entry, WITE::Database::Entry*>::make(&cubeInit),
  WITE::CallbackFactory<void, WITE::Database::Entry>::make(&cubeDestroy)
};

int main(int argc, char** argv) {
  WITE::WITE_INIT("WITE test cube");
  struct WITE::Shader::resourceLayoutEntry flatLayout = { SHADER_RESOURCE_UNIFORM, 0, 1, reinterpret_cast<void*>(sizeof(glm::dmat4)) };
  const char* flatFiles[2] = {"shaders/flat.vert.spv", "shaders/flat.frag.spv"};
  shaders.flat = WITE::Shader::make(flatFiles, 2, &flatLayout, 1);
  WITE::Database::registerType(cube::type, cube_functions);
  auto win1 = WITE::Window::make(0);
  WITE::IntBox3D bounds(100, 500, 100, 500);
  win1->setBounds(bounds);
  bounds = win1->getBounds();
  bounds.maxx -= bounds.minx;
  bounds.maxy -= bounds.miny;
  bounds.minx = bounds.miny = 0;
  //test
  bounds.minx += bounds.width = (bounds.maxx>>1);
  //endtest
  auto cam = win1->addCamera(bounds);
  cam->setFov(glm::radians(45.0f));
  cam->setMatrix(&glm::lookAt(glm::dvec3(-5, 3, -10), glm::dvec3(0, 0, 0), glm::dvec3(0, -1, 0)));
  cam->setLayermaks(~0);
  //cam->setFov(M_PI*0.25);
  WITE::Database db(1024 * 1024 * 1024);
  database = &db;
  db.allocate<cube>(cube::type);
  WITE::enterMainLoop();
}
