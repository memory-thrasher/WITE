#include <iostream>
// #include <cstring>
// #include <time.h>
// #include <signal.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../WITE/Database.hpp"
#include "../WITE/Window.hpp"
#include "../WITE/BackedRenderTarget.hpp"
#include "../WITE/StaticMeshRenderable.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShader.vert.spv.h"

using namespace WITE;
using namespace WITE::DB;
using namespace WITE::GPU;

#define GRU(x) GpuResource::USAGE_ ##x

constexpr logicalDeviceMask_t LDM_RENDERER = 1;
constexpr layer_t LAYER_PRIMARY = 0;

defineLiteralList(GpuResourceSlotInfo, COLOR_TARGET_REZ,
		  ImageSlotData(3, 8, 2, 1, 1, 1, LDM_RENDERER, GRU(ATT_OUTPUT), GRU(PRESENT)),
		  ImageSlotData(1, 16, 2, 1, 1, 1, LDM_RENDERER, GRU(ATT_DEPTH), 0));

defineLiteralList(usage_t, COLOR_TARGET_USAGES, GRU(ATT_OUTPUT), GRU(ATT_DEPTH));

defineLiteralListScalar(layer_t, PRIMARY_RENDER_LAYERS, 1);

defineLiteralList(RenderStep, COLOR_TARGET_STEPS, RenderStep(PRIMARY_RENDER_LAYERS, QueueType::eGraphics, COLOR_TARGET_USAGES));

constexpr BackedRenderTargetData COLOR_TARGET { COLOR_TARGET_REZ, COLOR_TARGET_STEPS };

#define FRAME_COUNT 500
#define transient reinterpret_cast<transients*>(dbe->transientData)

typedef Util::FrameSwappedResource<Buffer<BufferSlotData(LDM_RENDERER, GRU(DS_READ) | GRU(HOST_WRITE))>> TransformBuffer;

// constexpr auto basicRenderingPayloadData =
// 	    makeShaderData(
// constexpr std::array<size_t, 2> Vtemp = { 5, 5 };
// constexpr ShaderDataStorage<Vtemp> basicRenderingPayloadData {{
defineShaderData(basicRenderingPayloadData,
		 { { ShaderData::Vertex }, ShaderResourceProvider::eRenderable },
		 { { ShaderData::UniformToVertex }, ShaderResourceProvider::eRenderable }, //object transform
		 { { ShaderData::UniformToVertex }, ShaderResourceProvider::eRenderTarget }, //camera transform
		 { { ShaderData::ColorAttachment }, ShaderResourceProvider::eRenderTarget },
		 { { ShaderData::DepthAttachment }, ShaderResourceProvider::eRenderTarget }
		 );

ShaderModule basicShaderFrag { basicShader_frag, vk::ShaderStageFlagBits::eFragment },
  basicShaderVert { basicShader_vert, vk::ShaderStageFlagBits::eVertex };

VertexShader<basicRenderingPayloadData, VertexPrefab::basic3d> basicShader { &basicShaderVert, &basicShaderFrag };

constexpr Vertex<VertexPrefab::basic3d> pyramid_verts[] {
  { { 0, 0, 0 } }, { { 1, 1, 0 } }, { { 1, 0.5, 1 } },
    { { 1, 1, 0 } }, { { -1, 1, 0 } }, { { 1, 0.5, 1 } },
    { { -1, 1, 0 } }, { { 0, 0, 0 } }, { { 1, 0.5, 1 } },
    { { 1, 1, 0 } }, { { 0, 0, 0 } }, { { -1, 1, 0 } },
 };

typedef StaticMeshRenderable<basicRenderingPayloadData, VertexPrefab::basic3d, LDM_RENDERER, pyramid_verts> pyramid_mesh;

class pyramid_t {
private:
  struct transients {
    //TransformBuffer transformBuffer { GpuResourceInitData(sizeof(glm::dmat4)) };
    pyramid_mesh mesh { LAYER_PRIMARY, &basicShader };
  };
public:
  glm::dmat4 transform;
  static void onUpdate(DBRecord* data, DBEntity* dbe) {
    pyramid_t dis;
    dbe->completeRead(&dis, data);
    glm::rotate(dis.transform, glm::radians(1.0), glm::dvec3(0, 0, 1));
    //    transient->transformBuffer.getWrite().write(&dis.transform);
    dbe->write(&dis);
  };
  static void onSpinUp(DBEntity* dbe) {
    dbe->transientData = new transients();
  };
  static void onSpinDown(DBEntity* dbe) {
    delete transient;
  };
  static constexpr struct entity_type et = entity_type::makeFrom<pyramid_t>(1);
};

class camera_t {
private:
  uint64_t frameCounter;
  struct transients {
    Window window;
    BackedRenderTarget<COLOR_TARGET, LDM_RENDERER> canvas {
      BackingTupleInitData_cb_F::make([this](size_t idx, const GpuResourceSlotInfo& grsi){
	auto sz = this->window.getSize();
	return GpuResourceInitData(sz.width, sz.height);
      })};
  };
public:
  static void onUpdate(DBRecord* data, DBEntity* dbe) {
    Database* db = dbe->getDb();
    auto dbf = db->getFrame();
    camera_t dis;
    dbe->completeRead(&dis, data);
    auto frame = dis.frameCounter++;
    dbe->write(&dis);
    if(dbf >= FRAME_COUNT) db->shutdown();
    transient->canvas.render({});
    //always present what was rendered last frame, so don't render the first frame
    if(frame > 0) {
      auto* lastData = transient->canvas.getRead<0>();//0 = index of color attachment within resource list above
      transient->window.draw(lastData);
    }
  };
  static void onSpinUp(DBEntity* dbe) {
    dbe->transientData = new transients();
  };
  static void onSpinDown(DBEntity* dbe) {
    delete transient;
  };
  static constexpr struct entity_type et = entity_type::makeFrom<camera_t>(2);
};

struct entity_type types[] =
  {
    pyramid_t::et,
    camera_t::et
  };

const char* hw = "Hello World";
char out[200];

int main (int argc, char** argv) {
  Platform::Thread::init();
  float p = 1;
  Gpu::init(1, &p, "Test: Simple Render", {}, {});
  Database* db = new Database(types, sizeof(types)/sizeof(types[0]), 100);
  assert(db->allocate(camera_t::et));
  assert(db->allocate(pyramid_t::et));
  db->start();
  Gpu::shutdown();
  delete db;
  LOG("db deleted, exiting");
}

