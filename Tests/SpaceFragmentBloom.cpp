#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "vertexWholeScreen.vert.spv.h"
#include "fragmentSpace.frag.spv.h"
#include "fragmentBloom.frag.spv.h"

using namespace WITE;

constexpr uint64_t gpuId = 0;

/*
prefix acronyms:
BR buffer requirements
BRS buffer requirements - staging
IR image requirements
RR resource reference
RRL resource reference list
RMT resource map target
RMS resource map source
TL target layout
SL source layout
S shader
RP render pass
C copy step
L layer
IDL id list
 */

struct cameraData_t {
  glm::ivec4 gridOrigin; //xyz is origin sector
  glm::vec4 geometry;
  glm::vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
  glm::mat4 transform;
};

constexpr uint32_t skyboxPlaneCount = 8, starTypes = 31;

struct skyboxData_t {
  glm::ivec4 planeIncrement[skyboxPlaneCount];
  glm::ivec4 planeOffset[skyboxPlaneCount];
  glm::vec4 starTypeColors[starTypes];
};

skyboxData_t skyboxData {
  {
    { 30047, 8999, 283, 1<<24 },
    // { 30047, 283, 8999, 1<<24 },
    // { 8999, 30047, 283, 1<<24 },
    // { 283, 30047, 8999, 1<<24 },
    // { 8999, 283, 30047, 1<<24 },
    { 283, 8999, 30047, 1<<24 },
    // { -30047, 8999, 283, 1<<24 },
    // { -30047, 283, 8999, 1<<24 },
    // { -8999, 30047, 283, 1<<24 },
    { -283, 30047, 8999, 1<<24 },
    { -8999, 283, 30047, 1<<24 },
    // { -283, 8999, 30047, 1<<24 },
    // { 30047, -8999, 283, 1<<24 },
    { 30047, -283, 8999, 1<<24 },
    { 8999, -30047, 283, 1<<24 },
    // { 283, -30047, 8999, 1<<24 },
    // { 8999, -283, 30047, 1<<24 },
    // { 283, -8999, 30047, 1<<24 },
    // { 30047, 8999, -283, 1<<24 },
    // { 30047, 283, -8999, 1<<24 },
    { 8999, 30047, -283, 1<<24 },
    { 283, 30047, -8999, 1<<24 },
    // { 8999, 283, -30047, 1<<24 },
    // { 283, 8999, -30047, 1<<24 },
  },
  {
    { 25487, 9816, 24885, 0 },
    { 20349, 19407, 18975, 0 },
    { 24562, 9943, 6831, 0 },
    { 19378, 28973, 22316, 0 },
    { 2068, 29163, 21741, 0 },
    { 10640, 28147, 26672, 0 },
    { 69, 20071, 7573, 0 },
    { 16042, 1773, 21095, 0 },
    // { 8326, 17228, 30961, 0 },
    // { 13649, 24742, 22922, 0 },
    // { 5089, 6663, 21396, 0 },
    // { 3678, 29778, 4482, 0 },
    // { 10408, 5627, 1884, 0 },
    // { 18854, 18796, 17891, 0 },
    // { 27812, 28312, 11643, 0 },
    // { 14170, 21545, 21060, 0 },
    // { 7822, 26257, 31516, 0 },
    // { 9871, 12001, 5487, 0 },
    // { 13511, 30214, 25712, 0 },
    // { 3911, 10893, 6824, 0 },
    // { 13044, 23229, 31491, 0 },
    // { 20014, 31914, 4838, 0 },
    // { 20945, 6208, 27068, 0 },
    // { 5845, 8891, 23453, 0 },
  },
  // {},//normals filled in main
  {
    glm::vec4(0.6f, 0.69f, 1, 10 * 2),
    glm::vec4(0.57f, 0.71f, 1, 8 * 2),
    glm::vec4(0.667f, 0.749f, 1, 6 * 2),
    glm::vec4(0.635f, 0.753f, 1, 4 * 2),
    glm::vec4(0.792f, 0.843f, 1, 3 * 2),
    glm::vec4(0.835f, 0.878f, 1, 2 * 2),
    glm::vec4(0.973f, 0.969f, 1, 2 * 2),
    glm::vec4(1, 0.957f, 0.918f, 2),
    glm::vec4(1, 0.929f, 0.89f, 2),
    glm::vec4(1, 0.824f, 0.631f, 2),
    glm::vec4(1, 0.855f, 0.71f, 2),
    glm::vec4(1, 0.8f, 0.435f, 2),
    glm::vec4(1, 0.71f, 0.424f, 2),
    glm::vec4(1, 0.947f, 0.908f, 2),
    glm::vec4(1, 0.859f, 0.76f, 2),
    glm::vec4(1, 0.834f, 0.671f, 2),
    glm::vec4(1, 0.825f, 0.661f, 2),
    glm::vec4(1, 0.76f, 0.43f, 2),
    glm::vec4(1, 0.61f, 0.344f, 2),
    glm::vec4(0.792f, 0.843f, 1, 1),
    glm::vec4(0.835f, 0.878f, 1, 1),
    glm::vec4(0.973f, 0.969f, 1, 1),
    glm::vec4(1, 0.957f, 0.918f, 1),
    glm::vec4(1, 0.929f, 0.89f, 1),
    glm::vec4(1, 0.824f, 0.631f, 1),
    glm::vec4(1, 0.855f, 0.71f, 1),
    glm::vec4(1, 0.8f, 0.435f, 1),
    glm::vec4(1, 0.71f, 0.424f, 1),
    glm::vec4(1, 0.947f, 0.908f, 1),
    glm::vec4(1, 0.859f, 0.76f, 1),
    glm::vec4(1, 0.834f, 0.671f, 1)
  }
};

constexpr bufferRequirements BR_skyboxData = defineSimpleStorageBuffer(gpuId, sizeof(skyboxData_t));
constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BRS_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));

constexpr imageRequirements IR_intermediateColor = {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::RGBA32float,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
  .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  .frameswapCount = 1
};

constexpr imageRequirements IR_finalOutput = {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::RGBA32float,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
  .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
  .frameswapCount = 1
};

constexpr copyStep C_updateCameraData = defineCopy(),
	    C_all[] = {
	      C_updateCameraData
	    };

constexpr resourceReference RR_pass1_colorAttachment = defineSimpleColorReference(),
	    RR_pass2_colorAttachment = defineSimpleColorReference(),
	    RR_pass3_colorAttachment = defineSimpleColorReference(),
	    RR_pass2_sampler = defineSamplerReference(),
	    RR_pass3_sampler = defineSamplerReference(),
	    RR_pass1_cameraData = defineUBReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RR_pass1_skyboxData = defineSBReadonlyReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RRL_pass1[] = { RR_pass1_cameraData, RR_pass1_skyboxData },
	    RRL_pass2[] = { RR_pass2_sampler },
	    RRL_pass3[] = { RR_pass3_sampler };

constexpr uint64_t RR_IDL_cameraData[] = { RR_pass1_cameraData.id, C_updateCameraData.dst.id },
	    RR_IDL_pass1ColorAttachment[] = { RR_pass1_colorAttachment.id, RR_pass2_sampler.id },
	    RR_IDL_pass2ColorAttachment[] = { RR_pass2_colorAttachment.id, RR_pass3_sampler.id },
	    RR_IDL_pass3ColorAttachment[] = { RR_pass3_colorAttachment.id },
	    C_IDL_L1[] = { C_updateCameraData.id };

constexpr resourceMap RMT_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BRS_cameraData.id,
  .resourceReferences = C_updateCameraData.src.id
}, RMT_skyboxData = {
  .id = __LINE__,
  .requirementId = BR_skyboxData.id,
  .resourceReferences = RR_pass1_skyboxData.id,
  .external = true
}, RMT_pass3_color = {
  .id = __LINE__,
  .requirementId = IR_finalOutput.id,
  .resourceReferences = RR_IDL_pass3ColorAttachment,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true }
}, RMT_target[] = {
  RMT_cameraData_staging,
  RMT_skyboxData,
  RMT_pass3_color,
  {
    .id = __LINE__,
    .requirementId = BR_cameraData.id,
    .resourceReferences = RR_IDL_cameraData
  }, {
    .id = __LINE__,
    .requirementId = IR_intermediateColor.id,
    .resourceReferences = RR_IDL_pass2ColorAttachment,
    .resizeBehavior = { imageResizeType::eDiscard, {}, true }
  }, {
    .id = __LINE__,
    .requirementId = IR_intermediateColor.id,
    .resourceReferences = RR_IDL_pass1ColorAttachment,
    .resizeBehavior = { imageResizeType::eDiscard, {}, true }
  }
};

constexpr targetLayout TL_primary {
  .id = __LINE__,
  .resources = RMT_target, //pass by reference (with extra steps), so prior declaration is necessary
  .presentImageResourceMapId = RMT_pass3_color.id
};

defineShaderModules(S_spaceShaderModules,
		    { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
		    { fragmentSpace_frag, sizeof(fragmentSpace_frag), vk::ShaderStageFlagBits::eFragment });

constexpr graphicsShaderRequirements S_space {
  .id = __LINE__,
  .modules = S_spaceShaderModules,
  .targetProvidedResources = RRL_pass1,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3
};

constexpr renderPassRequirements RP_1 {
  .id = __LINE__,
  .color = RR_pass1_colorAttachment,
  .clearColor = true,
  .clearColorValue = { 0.0f, 0.0f, 0.0f, 1.0f },
  .shaders = S_space
};

constexpr int32_t fragmentBloomSpecData[2] = { 0, 1 };

constexpr vk::SpecializationMapEntry fragmentBloomSpecMap0 { 0, 0, 4 },
	    fragmentBloomSpecMap1 { 0, 4, 4 };

defineShaderModules(S_bloomShaderModules,
		    { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
		    {
		      .data = fragmentBloom_frag,
		      .size = sizeof(fragmentBloom_frag),
		      .stage = vk::ShaderStageFlagBits::eFragment,
		      .specializations = fragmentBloomSpecMap0,
		      .specializationData = (void*)fragmentBloomSpecData,
		      .specializationDataSize = sizeof(fragmentBloomSpecData)
		    });

constexpr graphicsShaderRequirements S_bloom {
  .id = __LINE__,
  .modules = S_bloomShaderModules,
  .targetProvidedResources = RRL_pass2,
  .vertexCountOverride = 3
};

constexpr renderPassRequirements RP_2 {
  .id = __LINE__,
  .color = RR_pass2_colorAttachment,
  .clearColor = true,
  .clearColorValue = { 0.0f, 0.0f, 0.0f, 1.0f },
  .shaders = S_bloom
};

defineShaderModules(S_bloomPart2ShaderModules,
		    { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
		    {
		      .data = fragmentBloom_frag,
		      .size = sizeof(fragmentBloom_frag),
		      .stage = vk::ShaderStageFlagBits::eFragment,
		      .specializations = fragmentBloomSpecMap1,
		      .specializationData = (void*)fragmentBloomSpecData,
		      .specializationDataSize = sizeof(fragmentBloomSpecData)
		    });

constexpr graphicsShaderRequirements S_bloomPart2 {
  .id = __LINE__,
  .modules = S_bloomPart2ShaderModules,
  .targetProvidedResources = RRL_pass3,
  .vertexCountOverride = 3
};

constexpr renderPassRequirements RP_3 {
  .id = __LINE__,
  .color = RR_pass3_colorAttachment,
  .clearColor = true,
  .clearColorValue = { 0.0f, 0.0f, 0.0f, 1.0f },
  .shaders = S_bloomPart2
};

constexpr renderPassRequirements allRPs[] { RP_1, RP_2, RP_3 };
constexpr uint64_t RP_IDL_L_1[] = { RP_1.id, RP_2.id, RP_3.id };

constexpr layerRequirements L_1 {
  .targetLayouts = TL_primary.id,
  .copies = C_IDL_L1,
  .renders = RP_IDL_L_1
};

constexpr imageRequirements allImageRequirements[] = {
  IR_intermediateColor,
  IR_finalOutput
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_skyboxData,
  BR_cameraData,
  BRS_cameraData
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .RPRS = allRPs,
  .CSS = C_all,
  .LRS = L_1,
  .TLS = TL_primary,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

int main(int argc, char** argv) {
  gpu::setOptions(argc, argv);
  gpu::init("Space test (fragment)");
  auto cliFov = gpu::getOption("fov");
  float fov = cliFov ? std::strtof(cliFov, NULL) : 45.0f;
  ASSERT_TRAP(fov > 0 && fov < 90, "invalid fov given on cli");
  primaryOnion = std::make_unique<onion_t>();
  buffer<BR_skyboxData> skyboxDataBuf;
  skyboxDataBuf.slowOutOfBandSet(skyboxData);
  auto camera = primaryOnion->createTarget<TL_primary.id>();
  camera->set<RMT_skyboxData.id>(&skyboxDataBuf);
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cd { { 1<<10, 1<<16, 1<<20, 0 }, { 0, glm::tan(glm::radians(fov)/size.y), 0.25f, 0 } };
  cd.clip.x = 0.1f;
  cd.clip.y = 100;
  cd.clip.z = glm::cot(glm::radians(fov/2));
  cd.clip.w = cd.clip.z * size.y / size.x;
  cd.transform = glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
  for(size_t i = 0;i < 10000;i++) {
    cd.gridOrigin.z = i;
    camera->write<RMT_cameraData_staging.id>(cd);
    primaryOnion->render();
  }
  WARN("sleeping...");
  WITE::Platform::Thread::sleep(1000);//waiting for render to finish. Should have onion destructor wait the active fence instead.
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
}

