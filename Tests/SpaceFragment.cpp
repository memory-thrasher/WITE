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
  glm::vec4 loc, norm, up, right;
  glm::vec4 size;
  glm::ivec4 gridOrigin; //xyz is origin sector, w is render distance (iterations)
};

constexpr uint32_t skyboxPlaneCount = 24;

struct skyboxData_t {
  glm::uvec4 size;
  glm::ivec4 data[skyboxPlaneCount * 2];
};

skyboxData_t skyboxData {
  glm::uvec4(skyboxPlaneCount, 0, 0, 0),
  {
    { 30047, 8999, 283, 1<<24 }, { 0, 1, 2, 16 },
    { 30047, 283, 8999, 1<<24 }, { 3, 4, 5, 16 },
    { 8999, 30047, 283, 1<<24 }, { 6, 7, 8, 16 },
    { 283, 30047, 8999, 1<<24 }, { 9, 10, 11, 16 },
    { 8999, 283, 30047, 1<<24 }, { 12, 13, 14, 16 },
    { 283, 8999, 30047, 1<<24 }, { 15, 16, 17, 16 },
    { -30047, 8999, 283, 1<<24 }, { 18, 19, 20, 16 },
    { -30047, 283, 8999, 1<<24 }, { 21, 22, 23, 16 },
    { -8999, 30047, 283, 1<<24 }, { 24, 25, 26, 16 },
    { -283, 30047, 8999, 1<<24 }, { 27, 28, 29, 16 },
    { -8999, 283, 30047, 1<<24 }, { 30, 31, 32, 16 },
    { -283, 8999, 30047, 1<<24 }, { 33, 34, 35, 16 },
    { 30047, -8999, 283, 1<<24 }, { 36, 37, 38, 16 },
    { 30047, -283, 8999, 1<<24 }, { 39, 40, 41, 16 },
    { 8999, -30047, 283, 1<<24 }, { 42, 43, 44, 16 },
    { 283, -30047, 8999, 1<<24 }, { 45, 46, 47, 16 },
    { 8999, -283, 30047, 1<<24 }, { 48, 49, 50, 16 },
    { 283, -8999, 30047, 1<<24 }, { 51, 52, 53, 16 },
    { 30047, 8999, -283, 1<<24 }, { 54, 55, 56, 16 },
    { 30047, 283, -8999, 1<<24 }, { 57, 58, 59, 16 },
    { 8999, 30047, -283, 1<<24 }, { 60, 61, 62, 16 },
    { 283, 30047, -8999, 1<<24 }, { 63, 64, 65, 16 },
    { 8999, 283, -30047, 1<<24 }, { 66, 67, 68, 16 },
    { 283, 8999, -30047, 1<<24 }, { 69, 70, 71, 16 },
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

constexpr layerRequirements L_1 {
  .targetLayouts = TL_primary.id,
  .copies = C_IDL_L1,
  .renders = RP_1.id
};

constexpr layerRequirements L_2 {
  .targetLayouts = TL_primary.id,
  .renders = RP_2.id
};

constexpr layerRequirements L_3 {
  .targetLayouts = TL_primary.id,
  .renders = RP_3.id
};

constexpr layerRequirements allLayers[] { L_1, L_2, L_3 };

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
  .LRS = allLayers,
  .TLS = TL_primary,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

constexpr float fov = 45.0f;

int main(int argc, char** argv) {
  gpu::setOptions(argc, argv);
  gpu::init("Space test (fragment)");
  primaryOnion = std::make_unique<onion_t>();
  buffer<BR_skyboxData> skyboxDataBuf;
  skyboxDataBuf.slowOutOfBandSet(skyboxData);
  auto camera = primaryOnion->createTarget<TL_primary.id>();
  camera->set<RMT_skyboxData.id>(&skyboxDataBuf);
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cd { { 0, 0, -5, 0 }, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }, { 1, 0, 0, 0 }, { size.x, size.y, glm::radians(fov)/size.y, 0 }, { 1<<10, 1<<16, 1<<20, 4 } };
  cd.size.w = std::tan(cd.size.z);
  for(size_t i = 0;i < 10000;i++) {
    cd.norm.x = std::sin(i/10000.0f);
    cd.norm.z = std::cos(i/10000.0f);
    cd.loc = cd.norm * -5.0f;
    cd.right = glm::vec4(glm::cross(glm::vec3(cd.up), glm::vec3(cd.norm)), 0);
    camera->write<RMT_cameraData_staging.id>(cd);
    primaryOnion->render();
  }
  WARN("sleeping...");
  WITE::Platform::Thread::sleep(1000);//waiting for render to finish. Should have onion destructor wait the active fence instead.
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
}

