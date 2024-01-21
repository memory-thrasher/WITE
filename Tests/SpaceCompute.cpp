#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "computeSpace.comp.spv.h"
#include "computeFlare.comp.spv.h"
#include "computeFlarePart2.comp.spv.h"

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

constexpr imageRequirements IR_standardColor = {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::RGBA32float,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
  .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,//transfer src is needed by window to present, transfer dst to clear (unless on a RP)
  .frameswapCount = 2
};

constexpr imageRequirements IR_flareTempColor = {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::RGBA32float,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
  .usage = vk::ImageUsageFlagBits::eStorage,
  .frameswapCount = 2
};

constexpr copyStep C_updateCameraData = defineCopy(),
	    C_all[] = {
	      C_updateCameraData
	    };

constexpr clearStep CL_flareTemp = defineClear(0, 0, 0, 0);

constexpr resourceReference RR_color = defineComputeColorReference(),
	    RR_color_flare = defineComputeColorReference(),
	    RR_flareTempColor = defineComputeColorReference(),
	    RR_color_flarePart2 = defineComputeColorReference(),
	    RR_flareTempColorPart2 = defineComputeColorReference(),
	    RR_cameraData = defineUBReferenceAtCompute(),
	    RR_skyboxData = defineSBReferenceAtCompute(),
	    RRL_flare[] = { RR_color_flare, RR_flareTempColor },
	    RRL_flarePart2[] = { RR_color_flarePart2, RR_flareTempColorPart2 },
	    RRL_camera[] = { RR_cameraData, RR_color, RR_skyboxData };//order here is binding idx in shader

constexpr uint64_t RR_IDL_cameraData[] = { RR_cameraData.id, C_updateCameraData.dst.id },
	    RR_IDL_color[] = { RR_color.id, RR_color_flare.id, RR_color_flarePart2.id },
	    RR_IDL_flareTempColor[] = { CL_flareTemp.id, RR_flareTempColor.id, RR_flareTempColorPart2.id },
	    C_IDL_L1[] = { C_updateCameraData.id };

constexpr resourceMap RMT_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BRS_cameraData.id,
  .resourceReferences = C_updateCameraData.src.id
}, RMT_skyboxData = {
  .id = __LINE__,
  .requirementId = BR_skyboxData.id,
  .resourceReferences = RR_skyboxData.id,
  .external = true
}, RMT_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .resourceReferences = RR_IDL_color,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true }
}, RMT_flareTemp = {
  .id = __LINE__,
  .requirementId = IR_flareTempColor.id,
  .resourceReferences = RR_IDL_flareTempColor,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true }
}, RMT_target[] = {
  RMT_cameraData_staging,
  RMT_skyboxData,
  RMT_color,
  RMT_flareTemp,
  {
    .id = __LINE__,
    .requirementId = BR_cameraData.id,
    .resourceReferences = RR_IDL_cameraData
  }};

constexpr targetLayout TL_standardRender {
  .id = __LINE__,
  .resources = RMT_target, //pass by reference (with extra steps), so prior declaration is necessary
  .presentImageResourceMapId = RMT_color.id
};

constexpr shaderModule spaceShaderModule { computeSpace_comp, sizeof(computeSpace_comp), vk::ShaderStageFlagBits::eCompute };

constexpr computeShaderRequirements S_space {
  .id = __LINE__,
  .module = &spaceShaderModule,
  .targetProvidedResources = RRL_camera,//layout set 0 because there are no sources
  .primaryOutputReferenceId = RR_color.id,
  .strideX = 30,
  .strideY = 30
};

constexpr shaderModule flareShaderModule { computeFlare_comp, sizeof(computeFlare_comp), vk::ShaderStageFlagBits::eCompute };

constexpr computeShaderRequirements S_flare {
  .id = __LINE__,
  .module = &flareShaderModule,
  .targetProvidedResources = RRL_flare,//layout set 0 because there are no sources
  .primaryOutputReferenceId = RR_color_flare.id,
  .strideX = 128,
  .strideY = 99999
};

constexpr shaderModule flarePart2ShaderModule { computeFlarePart2_comp, sizeof(computeFlarePart2_comp), vk::ShaderStageFlagBits::eCompute };

constexpr computeShaderRequirements S_flarePart2 {
  .id = __LINE__,
  .module = &flarePart2ShaderModule,
  .targetProvidedResources = RRL_flarePart2,//layout set 0 because there are no sources
  .primaryOutputReferenceId = RR_color_flare.id,
  .strideX = 99999,
  .strideY = 128
};

constexpr uint64_t IDL_computeShaders[] = {
  S_space.id,
  S_flare.id,
  S_flarePart2.id,
};

constexpr computeShaderRequirements S_L_computeShaders[] { S_space, S_flare, S_flarePart2 };

constexpr layerRequirements L_1 {
  .targetLayouts = TL_standardRender.id,
  .clears = CL_flareTemp.id,
  .copies = C_IDL_L1,
  .computeShaders = IDL_computeShaders
};

constexpr imageRequirements allImageRequirements[] = {
  IR_standardColor,
  IR_flareTempColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_skyboxData,
  BR_cameraData,
  BRS_cameraData
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .CSRS = S_L_computeShaders,
  .CLS = CL_flareTemp,
  .CSS = C_all,
  .LRS = L_1,
  .TLS = TL_standardRender,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

constexpr float fov = 45.0f;

int main(int argc, char** argv) {
  gpu::init("Space test");
  primaryOnion = std::make_unique<onion_t>();
  buffer<BR_skyboxData> skyboxDataBuf;
  skyboxDataBuf.slowOutOfBandSet(skyboxData);
  auto camera = primaryOnion->createTarget<TL_standardRender.id>();
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

