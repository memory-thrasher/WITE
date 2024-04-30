#include "../WITE/WITE.hpp"

//begin shared
constexpr uint64_t gpuId = 0;

//begin text
#include "shared/font.hpp"

wrap_mesh_fromArray(gpuId, WITE::UDM::RG8uint, fontMesh, font_triangles);

constexpr WITE::objectLayout OL_text = { .id = __LINE__ };

struct drawData_t {//both drawdata in IndirectText.vert and color in soliddColor.frag
  glm::vec4 color, bbox, charMetric;
};

constexpr WITE::bufferRequirements BR_string = defineSimpleStorageBuffer(gpuId, 1024),
	    BR_S_string = NEW_ID(stagingRequirementsFor(BR_string, 2)),
	    BR_charData = defineSimpleStorageBuffer(gpuId, sizeof(font_character_extents)),
	    BR_textIndirect = {
	      .deviceId = gpuId,
	      .id = __LINE__,
	      .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,//compute->vertex
	      .size = 16 + 1024*4*4,//uvec4 char count + 1024 characters * 4 * uint32 each (indirect draw command)
	      .frameswapCount = 1,
	      .hostVisible = false,
	    },
	    BR_drawData = defineSimpleUniformBuffer(gpuId, sizeof(drawData_t)),
	    BR_S_drawData = NEW_ID(stagingRequirementsFor(BR_drawData, 2));

constexpr WITE::resourceSlot RS_text_string = {
  .id = __LINE__,
  .requirementId = BR_string.id,
  .objectLayoutId = OL_text.id,
}, RS_text_S_string = {
  .id = __LINE__,
  .requirementId = BR_S_string.id,
  .objectLayoutId = OL_text.id,
}, RS_text_charData = {
  .id = __LINE__,
  .requirementId = BR_charData.id,
  .objectLayoutId = OL_text.id,
  .external = true,
}, RS_text_indirect = {
  .id = __LINE__,
  .requirementId = BR_textIndirect.id,
  .objectLayoutId = OL_text.id,
}, RS_text_drawData = {
  .id = __LINE__,
  .requirementId = BR_drawData.id,
  .objectLayoutId = OL_text.id,
}, RS_text_S_drawData = {
  .id = __LINE__,
  .requirementId = BR_S_drawData.id,
  .objectLayoutId = OL_text.id,
}, RS_text_mesh = {
  .id = __LINE__,
  .requirementId = fontMesh.bufferRequirements_v.id,
  .objectLayoutId = OL_text.id,
  .external = true,
}, RS_L_text[] = {
  RS_text_string,
  RS_text_S_string,
  RS_text_charData,
  RS_text_indirect,
  RS_text_drawData,
  RS_text_S_drawData,
  RS_text_mesh,
};

//begin text/compute
#include "Tests/IndirectText.comp.spv.h"

constexpr WITE::resourceConsumer RC_S_textCompute_string = defineSBReadonlyConsumer(Compute),
	    RC_S_textCompute_charData = defineSBReadonlyConsumer(Compute),
	    RC_S_textCompute_indirect = defineSBWriteonlyConsumer(Compute),
	    RC_L_S_textCompute[] = { RC_S_textCompute_string, RC_S_textCompute_charData, RC_S_textCompute_indirect };

constexpr WITE::shaderModule SM_textCompute { IndirectText_comp, sizeof(IndirectText_comp), vk::ShaderStageFlagBits::eCompute };

constexpr WITE::computeShaderRequirements S_textCompute {
  .id = __LINE__,
  .module = &SM_textCompute,
  .sourceProvidedResources = RC_L_S_textCompute,
  .primaryOutputSlotId = RS_text_string.id,
  .strideX = 64,
};

//begin text/graphics
#include "Tests/IndirectText.vert.spv.h"
#include "Tests/solidColor.frag.spv.h"

constexpr WITE::resourceConsumer RC_S_textRender_drawData = defineUBConsumer(Vertex | vk::ShaderStageFlagBits::eFragment),
	    RC_S_textRender_indirect = defineIndirectConsumer(),
	    RC_S_textRender_indirectCount = defineIndirectCountConsumer(),
	    RC_L_S_textRender[] = { RC_S_textRender_drawData, fontMesh.resourceConsumer_v, RC_S_textRender_indirect, RC_S_textRender_indirectCount };

constexpr WITE::shaderModule SM_L_textRender[] {
  { IndirectText_vert, sizeof(IndirectText_vert), vk::ShaderStageFlagBits::eVertex },
  { solidColor_frag, sizeof(solidColor_frag), vk::ShaderStageFlagBits::eFragment }
};

constexpr WITE::graphicsShaderRequirements S_textRender {
  .id = __LINE__,
  .modules = SM_L_textRender,
  .sourceProvidedResources = RC_L_S_textRender,
  .cullMode = vk::CullModeFlagBits::eNone,
};

//begin text/linkage
constexpr WITE::copyStep CP_strings = defineCopy(),
	    CP_drawData = defineCopy();

constexpr WITE::resourceReference RR_L_text[] = {
  { .resourceConsumerId = RC_S_textCompute_string.id, .resourceSlotId = RS_text_string.id },
  { .resourceConsumerId = RC_S_textCompute_charData.id, .resourceSlotId = RS_text_charData.id },
  { .resourceConsumerId = RC_S_textCompute_indirect.id, .resourceSlotId = RS_text_indirect.id },
  { .resourceConsumerId = RC_S_textRender_indirectCount.id, .resourceSlotId = RS_text_indirect.id, .subresource = {0, 16} },
  { .resourceConsumerId = RC_S_textRender_indirect.id, .resourceSlotId = RS_text_indirect.id, .subresource = {16} },
  { .resourceConsumerId = RC_S_textRender_drawData.id, .resourceSlotId = RS_text_drawData.id },
  { .resourceConsumerId = fontMesh.resourceConsumer_v.id, .resourceSlotId = RS_text_mesh.id },
  { .resourceConsumerId = CP_strings.src, .resourceSlotId = RS_text_S_string.id },
  { .resourceConsumerId = CP_strings.dst, .resourceSlotId = RS_text_string.id },
  { .resourceConsumerId = CP_drawData.src, .resourceSlotId = RS_text_S_drawData.id },
  { .resourceConsumerId = CP_drawData.dst, .resourceSlotId = RS_text_drawData.id },
};

constexpr WITE::sourceLayout SL_text = {
  .id = __LINE__,
  .objectLayoutId = OL_text.id,
  .resources = RR_L_text,
};

//begin camera
constexpr uint64_t RC_ID_present_color = __LINE__;

constexpr WITE::objectLayout OL_camera = {
  .id = __LINE__,
  .windowConsumerId = RC_ID_present_color,//this implicitly creates a window that presents the image here by RR
};

constexpr WITE::imageRequirements IR_camera_color = defineSimpleColor(gpuId);

constexpr WITE::resourceSlot RS_camera_color = {
  .id = __LINE__,
  .requirementId = IR_camera_color.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = WITE::resize_trackWindow_discard,
};

constexpr WITE::resourceReference RR_L_camera[] = {
  { .resourceConsumerId = RC_ID_present_color, .resourceSlotId = RS_camera_color.id },
};

constexpr WITE::targetLayout TL_camera = {
  .id = __LINE__,
  .objectLayoutId = OL_camera.id,
  .resources = RR_L_camera,
};

constexpr WITE::renderPassRequirements RP_1 = {
  .id = __LINE__,
  .color = RC_ID_present_color,
  .clearColor = true,
  .clearColorValue = { 0, 0, 0, 0 },
  .shaders = S_textRender,
};

constexpr uint64_t CP_IDL_all[] = { CP_strings.id, CP_drawData.id };

constexpr WITE::layerRequirements LRS[] = {
  {
    .copies = CP_IDL_all,
    .computeShaders = S_textCompute.id,
  }, {//two layers because compute must come before render.
    .renders = RP_1.id,
  }
};

//begin onion
constexpr WITE::bufferRequirements allBRS[] = {
  BR_drawData,
  BR_textIndirect,
  BR_charData,
  BR_S_drawData,
  BR_S_string,
  BR_string,
  fontMesh.bufferRequirements_v,
};

constexpr auto RS_all = WITE::concat<WITE::resourceSlot, RS_L_text, RS_camera_color>();

constexpr WITE::copyStep CP_all[] = { CP_strings, CP_drawData };

constexpr WITE::objectLayout allOLS[] = { OL_text, OL_camera };

constexpr WITE::onionDescriptor od = {
  .IRS = IR_camera_color,
  .BRS = allBRS,
  .RSS = RS_all,
  .CSRS = S_textCompute,
  .RPRS = RP_1,
  .CSS = CP_all,
  .LRS = LRS,
  .OLS = allOLS,
  .TLS = TL_camera,
  .SLS = SL_text,
  .GPUID = gpuId,
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

void onion_printf(onion_t::object_t<OL_text.id>* text, const char* fmt, ...) {
  WITE::copyableArray<char, 1024> buf;
  memset(buf.ptr(), ' ', 1024);//pad with spaces removes some otherwise wasted compute shader time
  va_list args;
  va_start(args, fmt);
  vsprintf(buf.ptr(), fmt, args);
  va_end(args);
  text->write<RS_text_S_string.id>(buf);
};

int main(int argc, char** argv) {
  WITE::configuration::setOptions(argc, argv);
  WITE::gpu::init("Input and combined procedural and traditional render test");
  WITE::winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->create<OL_camera.id>();
  auto textMeshBuf = fontMesh.spawnMeshBuffer();
  WITE::buffer<BR_charData> characterData;
  characterData.slowOutOfBandSet(font_character_extents);
  auto text = primaryOnion->create<OL_text.id>();
  text->set<RS_text_charData.id>(&characterData);
  text->set<RS_text_mesh.id>(textMeshBuf.get());
  glm::vec2 size = camera->getWindow().getVecSize();
  float fontSizePxl = 28;
  drawData_t drawData = {
    { 1, 1, 1, 1 },
    { 0.1f, 0.1f, 0.2f, 0.8f },
    { fontSizePxl / size.x, fontSizePxl / size.y, 0, 0 },
  };
  for(size_t i = 0;i < 10000 && !WITE::shutdownRequested();i++) {
    WITE::winput::pollInput();
    onion_printf(text, "Hello World! Frame: %d  the quick brown fox jumped over the lazy dog. THE QUICK BROWN FOX JUMPED OVER THE LAZY DOG? !\"#$%%&'()*+,-./0123456789:;<=>?@[\\]^_`{|}~", i);
    text->write<RS_text_S_drawData.id>(drawData);
    primaryOnion->render();
  }
  primaryOnion.reset();
};

