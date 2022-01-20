#include <glm/glm.hpp>

#include "../WaffleIronTransactionalEngine/Export.h"
#include "cube_data.h"

struct cube {
  constexpr static WITE::Database::type type = 12;
  static WITE::StaticMesh mesh;
  WITE::Transform trans;
};

#define cubeMesh g_vb_solid_face_colors_Data
WITE::StaticMesh cube::mesh(cubeMesh, sizeof(cubeMesh)/sizeof(cubeMesh[0]));
WITE::StaticMesh* monkeyMesh;

typedef struct {
  WITE::Shader* flat;
} shaders_t;

static shaders_t shaders;

static uint64_t frameIdx = 0;

static struct {
  std::shared_ptr<WITE::RenderPass> cam1;
} renderPasses;

void cubeUpdate(WITE::Database::Entry e) {
  frameIdx++;
  auto o = (*database->getObjectInstanceFor(e));
  WITE::Transform trans(glm::dvec3(0, 0, 0), glm::dvec3(0, -glm::half_pi<double>(), frameIdx * glm::pi<double>() / 60.0 - glm::half_pi<double>()));
  o->pushTrans(&trans);
  if(frameIdx % 1000 == 0) {
    LOG("living time: %llu\n", WITE::Time::frame() - WITE::Time::launchTime());
  }
  if(WITE::Time::launchTime() + 10000000000 <= WITE::Time::frame()) {
    //if(frameIdx > 1000) {
    LOG("Delta nano: %llu\nFPS: %llu\nlife: %llu\nframes: %llu\n", WITE::Time::delta(), frameIdx/10, WITE::Time::frame() - WITE::Time::launchTime(), frameIdx);
    WITE::gracefulExit();
  }
}

void cubeEveryRenderer(WITE::Renderer* renderer, std::shared_ptr<class WITE::ShaderResource>* resources, WITE::GPU* gpu) {
  *reinterpret_cast<glm::mat4*>(resources[1]->map()) = renderer->getObj()->getTrans().getInvMat();
  resources[1]->unmap();
  renderer->updateInstanceData(1, gpu);
  //LOGMAT(renderer->getObj()->getTrans().getInvMat(), "inverse monkey");
}

void cubeInit(WITE::Database::Entry e, WITE::Database::Entry* map) {
  auto o = (*database->getObjectInstanceFor(e)) = WITE::Object::make(e, offsetof(struct cube, trans), map);
  o->setName("Protea");
  WITE::Renderer::bind(o, shaders.flat, WITE::Mesh::make(monkeyMesh), 0);
  WITE::Transform initialTrans(glm::dvec3(0, 0, 0), glm::dvec3(0, -glm::half_pi<double>(), -glm::half_pi<double>()));
  o->pushTrans(&initialTrans);
  o->getRenderer(0)->setOnceCallback(WITE::Renderer::packDataCB_F::make(&cubeEveryRenderer));
  o->getRenderer(0)->setPerFrameCallback(WITE::Renderer::packDataCB_F::make(&cubeEveryRenderer));
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
  //start test monkey
  std::vector<WITE::Vertex> monkeyVerts;
  FILE* monkeyFile = fopen("models/test_monkey.obj", "rb");
  WITE::StaticMesh::ImportObj(monkeyFile, &monkeyVerts);
  monkeyMesh = new WITE::StaticMesh(monkeyVerts.data(), (uint32_t)monkeyVerts.size());
  fclose(monkeyFile);
  //end test monkey
  WITE::WITE_INIT("WITE test cube", DEBUG_MASK_VULKAN);
  struct WITE::Shader::resourceLayoutEntry flatLayout[] = {
    { SHADER_RESOURCE_UNIFORM, SHADER_STAGE_VERT, 1, reinterpret_cast<void*>(sizeof(glm::mat4)) },
    { SHADER_RESOURCE_UNIFORM, SHADER_STAGE_FRAG, 1, reinterpret_cast<void*>(sizeof(glm::mat4)) }
  };
  const char* flatFiles[2] = {"shaders/flat.vert.spv", "shaders/flat.frag.spv"};
  shaders.flat = WITE::Shader::make(flatFiles, 2, flatLayout, 2);
  WITE::Database::registerType(cube::type, cube_functions);
  auto win1 = WITE::Window::make(0);
  //WITE::IntBox3D bounds(100, 600, 100, 600);
  //win1->setBounds(bounds);
  auto bounds = win1->getBounds();
  bounds.maxx -= bounds.minx;
  bounds.maxy -= bounds.miny;
  bounds.minx = bounds.miny = 0;
  renderPasses.cam1 = WITE::RenderPass::make(win1->getGraphicsQueue());
  renderPasses.cam1->setLayermask(~0);//this should also be the default
  auto cam = win1->addCamera(bounds, renderPasses.cam1);
  auto cam = WITE::Camera::make(bounds);
  cam->setFov(glm::radians(45.0f) * bounds.height() / bounds.width());
  cam->setMatrix(&glm::lookAtRH(glm::dvec3(20, 0, 12), glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 1)));
  auto db = WITE::Database::makeDatabase(1024 * 1024 * 1024);
  database = db.get();
  db->allocate<cube>(cube::type);
  WITE::enterMainLoop();
  db->gracefulStop();
}
