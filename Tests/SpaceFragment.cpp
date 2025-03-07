/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "Tests/vertexWholeScreen.vert.spv.h"
#include "Tests/fragmentSpace.frag.spv.h"
#include "Tests/fragmentBloom.frag.spv.h"

static_assert(sizeof(vertexWholeScreen_vert) > 0);
static_assert(sizeof(fragmentSpace_frag) > 0);
static_assert(sizeof(fragmentBloom_frag) > 0);

using namespace WITE;

/*
prefix acronyms:
BR buffer requirements
IR image requirements
RS resource slot
RC resource consumer
RR resource reference
TL target layout
SL source layout
OL object layout
S shader
SM shader module
RP render pass
CP copy step
CL clear step
L layer
*_L *list
*_ID *id
*_S *staging

BR/IR_dataDescription
RC_consumerName_usageDescription
RS_objectName_dataDescription
RR_L_object_layout
SL/TL_objectName_layerDescription
S_shaderName
SM_L_shaderName
L/RP_id
CL/CP_dataDescription

 */

//begin shared
constexpr uint64_t gpuId = 0;
constexpr bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr bufferRequirements BR_S_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr copyStep CP_transform = defineCopy();
constexpr uint64_t RC_ID_present_color = __LINE__,
	    RC_ID_RP_1_color = __LINE__;

//end shared
//begin space
constexpr objectLayout OL_space = { .id = __LINE__ };

constexpr uint32_t spacePlaneCount = 8, starTypes = 31;

struct spaceData_t {
  glm::ivec4 planeIncrement[spacePlaneCount];
  glm::ivec4 planeOffset[spacePlaneCount];
  glm::vec4 starTypeColors[starTypes];
};

constexpr spaceData_t spaceData {
  {
    { 30047, 8999, 283, 1<<24 },
    { 283, 8999, 30047, 1<<24 },
    { -283, 30047, 8999, 1<<24 },
    { -8999, 283, 30047, 1<<24 },
    { 30047, -283, 8999, 1<<24 },
    { 8999, -30047, 283, 1<<24 },
    { 8999, 30047, -283, 1<<24 },
    { 283, 30047, -8999, 1<<24 },
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
  },
  {
    { 0.6f, 0.69f, 1, 10 * 2 },
    { 0.57f, 0.71f, 1, 8 * 2 },
    { 0.667f, 0.749f, 1, 6 * 2 },
    { 0.635f, 0.753f, 1, 4 * 2 },
    { 0.792f, 0.843f, 1, 3 * 2 },
    { 0.835f, 0.878f, 1, 2 * 2 },
    { 0.973f, 0.969f, 1, 2 * 2 },
    { 1, 0.957f, 0.918f, 2 },
    { 1, 0.929f, 0.89f, 2 },
    { 1, 0.824f, 0.631f, 2 },
    { 1, 0.855f, 0.71f, 2 },
    { 1, 0.8f, 0.435f, 2 },
    { 1, 0.71f, 0.424f, 2 },
    { 1, 0.947f, 0.908f, 2 },
    { 1, 0.859f, 0.76f, 2 },
    { 1, 0.834f, 0.671f, 2 },
    { 1, 0.825f, 0.661f, 2 },
    { 1, 0.76f, 0.43f, 2 },
    { 1, 0.61f, 0.344f, 2 },
    { 0.792f, 0.843f, 1, 1 },
    { 0.835f, 0.878f, 1, 1 },
    { 0.973f, 0.969f, 1, 1 },
    { 1, 0.957f, 0.918f, 1 },
    { 1, 0.929f, 0.89f, 1 },
    { 1, 0.824f, 0.631f, 1 },
    { 1, 0.855f, 0.71f, 1 },
    { 1, 0.8f, 0.435f, 1 },
    { 1, 0.71f, 0.424f, 1 },
    { 1, 0.947f, 0.908f, 1 },
    { 1, 0.859f, 0.76f, 1 },
    { 1, 0.834f, 0.671f, 1 },
  }
};

constexpr bufferRequirements BR_spaceData = defineSimpleStorageBuffer(gpuId, sizeof(spaceData_t));

constexpr resourceConsumer RC_S_space_cameraTrans = defineUBConsumer(Fragment),
	    RC_S_space_cameraData = defineUBConsumer(Fragment),
	    RC_S_space_modelData = defineSBReadonlyConsumer(Fragment);

constexpr resourceConsumer RC_L_S_space_target[] = {
  RC_S_space_cameraData,
  RC_S_space_cameraTrans,
};

constexpr resourceConsumer RC_L_S_space_source[] = {
  RC_S_space_modelData,
};

constexpr shaderModule SM_L_space[] = {
  { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
  { fragmentSpace_frag, sizeof(fragmentSpace_frag), vk::ShaderStageFlagBits::eFragment }
};

constexpr graphicsShaderRequirements S_space {
  .id = __LINE__,
  .modules = SM_L_space,
  .targetProvidedResources = RC_L_S_space_target,
  .sourceProvidedResources = RC_L_S_space_source,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3
};

constexpr resourceSlot RS_space_spaceData = {
  .id = __LINE__,
  .requirementId = BR_spaceData.id,
  .objectLayoutId = OL_space.id,
  .external = true,
};

constexpr resourceReference RR_L_space[] = {
  { .resourceConsumerId = RC_S_space_modelData.id, .resourceSlotId = RS_space_spaceData.id },
};

constexpr sourceLayout SL_space = {
  .id = __LINE__,
  .objectLayoutId = OL_space.id,
  .resources = RR_L_space,
};

//end space
//begin camera
struct cameraData_t {
  glm::vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
  glm::ivec4 gridOrigin; //xyz is origin sector
  glm::vec4 geometry;
};

constexpr objectLayout OL_camera = {
  .id = __LINE__,
  .windowConsumerId = RC_ID_present_color,//this implicitly creates a window that presents the image here by RR
};

constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BR_S_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));

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
  .format = Format::RGBA8unorm,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
  .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
  .frameswapCount = 1
};

constexpr resourceConsumer RC_bloomH_inColor = defineSamplerConsumer(Fragment);

defineShaderSpecialization(int32_t, 0, S_bloomH_specialization);

constexpr shaderModule SM_L_bloomH[] = {
  { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
  { fragmentBloom_frag, sizeof(fragmentBloom_frag), vk::ShaderStageFlagBits::eFragment, forwardShaderSpecialization(S_bloomH_specialization) },
};

constexpr graphicsShaderRequirements S_bloomH {
  .id = __LINE__,
  .modules = SM_L_bloomH,
  .targetProvidedResources = RC_bloomH_inColor,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3
};

constexpr resourceConsumer RC_bloomV_inColor = defineSamplerConsumer(Fragment);

defineShaderSpecialization(int32_t, 1, S_bloomV_specialization);

constexpr shaderModule SM_L_bloomV[] = {
  { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
  { fragmentBloom_frag, sizeof(fragmentBloom_frag), vk::ShaderStageFlagBits::eFragment, forwardShaderSpecialization(S_bloomV_specialization) },
};

constexpr graphicsShaderRequirements S_bloomV {
  .id = __LINE__,
  .modules = SM_L_bloomV,
  .targetProvidedResources = RC_bloomV_inColor,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3
};

constexpr uint64_t RC_ID_RP_bloomH_outColor = __LINE__,
	    RC_ID_RP_bloomV_outColor = __LINE__;

constexpr copyStep CP_camera_cameraData = defineCopy();

constexpr resourceSlot RS_camera_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BR_S_cameraData.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_cameraData = {
  .id = __LINE__,
  .requirementId = BR_cameraData.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_trans_staging = {
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_color_raw = {
  .id = __LINE__,
  .requirementId = IR_intermediateColor.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = resize_trackWindow_discard,
}, RS_camera_color_bloomH = {
  .id = __LINE__,
  .requirementId = IR_intermediateColor.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = resize_trackWindow_discard,
}, RS_camera_color_bloomFinal = {
  .id = __LINE__,
  .requirementId = IR_finalOutput.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = resize_trackWindow_discard,
};

constexpr resourceReference RR_L_camera[] = {
  { .resourceConsumerId = CP_camera_cameraData.src, .resourceSlotId = RS_camera_cameraData_staging.id },
  { .resourceConsumerId = CP_camera_cameraData.dst, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_camera_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_S_space_cameraData.id, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = RC_S_space_cameraTrans.id, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_ID_RP_1_color, .resourceSlotId = RS_camera_color_raw.id },
  { .resourceConsumerId = RC_bloomH_inColor.id, .resourceSlotId = RS_camera_color_raw.id },
  { .resourceConsumerId = RC_ID_RP_bloomH_outColor, .resourceSlotId = RS_camera_color_bloomH.id },
  { .resourceConsumerId = RC_bloomV_inColor.id, .resourceSlotId = RS_camera_color_bloomH.id },
  { .resourceConsumerId = RC_ID_RP_bloomV_outColor, .resourceSlotId = RS_camera_color_bloomFinal.id },
  { .resourceConsumerId = RC_ID_present_color, .resourceSlotId = RS_camera_color_bloomFinal.id },
};

constexpr targetLayout TL_camera = {
  .id = __LINE__,
  .objectLayoutId = OL_camera.id,
  .resources = RR_L_camera, //pass by reference (with extra steps), so prior declaration is necessary
};

//end camera
//begin layer
constexpr graphicsShaderRequirements S_RP_1[] = { S_space };

constexpr renderPassRequirements RP_1 {
  .id = __LINE__,
  .color = RC_ID_RP_1_color,
  .clearColor = true,
  .clearColorValue = { 0.1f, 0.1f, 0.1f, 1.0f },
  .shaders = S_RP_1,
};

constexpr renderPassRequirements RP_bloomH {
  .id = __LINE__,
  .color = RC_ID_RP_bloomH_outColor,
  .shaders = S_bloomH,
};

constexpr renderPassRequirements RP_bloomV {
  .id = __LINE__,
  .color = RC_ID_RP_bloomV_outColor,
  .shaders = S_bloomV,
};

constexpr uint64_t RP_ID_L_L1[] = { RP_1.id, RP_bloomH.id, RP_bloomV.id };

constexpr uint64_t CP_ID_L_L1[] = { CP_transform.id, CP_camera_cameraData.id };

constexpr layerRequirements L_1 {
  .copies = CP_ID_L_L1,
  .renders = RP_ID_L_L1,
};

//end layer
//begin OD
constexpr copyStep CP_all[] = {
  CP_camera_cameraData,
  CP_transform,
};

constexpr sourceLayout SL_all[] = {
  SL_space,
};

constexpr objectLayout OL_L_all[] = {
  OL_space,
  OL_camera,
};

constexpr imageRequirements allImageRequirements[] = {
  IR_intermediateColor,
  IR_finalOutput,
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_singleTransform,
  BR_S_singleTransform,
  BR_S_cameraData,
  BR_cameraData,
  BR_spaceData,
};

constexpr renderPassRequirements RP_L_all[] = {
  RP_bloomV,
  RP_bloomH,
  RP_1,
};

constexpr resourceSlot RS_L_all[] = {
  RS_camera_trans,
  RS_camera_trans_staging,
  RS_camera_cameraData,
  RS_camera_cameraData_staging,
  RS_camera_color_raw,
  RS_camera_color_bloomH,
  RS_camera_color_bloomFinal,
  RS_space_spaceData,
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .RSS = RS_L_all,
  .RPRS = RP_L_all,
  .CSS = CP_all,
  .LRS = L_1,
  .OLS = OL_L_all,
  .TLS = TL_camera,
  .SLS = SL_all,
  .GPUID = gpuId
};

//end OD

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

const float fov = 45;

//540fps
int main(int argc, const char** argv) {
  configuration::setOptions(argc, argv);
  gpu::init("Input and combined procedural and traditional render test");
  winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->create<OL_camera.id>();
  buffer<BR_spaceData> spaceDataBuf;
  spaceDataBuf.slowOutOfBandSet(spaceData);
  auto space = primaryOnion->create<OL_space.id>();
  space->set<RS_space_spaceData.id>(&spaceDataBuf);
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cd { {}, { 1<<10, 1<<16, 1<<20, 0 }, { 0, glm::tan(glm::radians(fov)/size.y), 0.25f, 0 } };
  cd.clip.x = 0.1f;
  cd.clip.y = 100;
  cd.clip.z = glm::cot(glm::radians(fov/2));
  cd.clip.w = cd.clip.z * size.y / size.x;
  for(size_t i = 0;i < 10000 && !shutdownRequested();i++) {
    winput::pollInput();
    cd.gridOrigin.z = i;
    camera->write<RS_camera_cameraData_staging.id>(cd);
    camera->write<RS_camera_trans_staging.id>(glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)));
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
  primaryOnion.reset();
}
