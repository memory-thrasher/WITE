#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "vertexWholeScreen.vert.spv.h"
#include "fragmentSpace.frag.spv.h"
#include "box_normal.vert.spv.h"
#include "reflective.frag.spv.h"

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
RML[S|T] resource map list [source|target]
TL target layout
SL source layout
S shader
RP render pass
C copy step
L layer
*_IDL id list

naming convention:
{BR|BRS|IR|IRS}_{contentType}[_{target|source|pass} if single use]
RR_{shader|pass}_{contentType}
RRL_{shader|pass}_{"targets"|"sources"}
RR_IDL_{contentType}_common
RR_IDL_{target|source}_{contentType}
RMT_{target}_{contentType}[_staging]
RMS_{source}_{contentType}[_staging]
RMLS_{source}
RMLT_{target}
TL_{target}
SL_{source}
S_{shader}
RP_{pass}
C_{contentType}
L_{layer}

in this case:
contentType: singleTransform, skyboxSeedData, cameraData
target: finalOutput, reflectiveCubemap
source: reflectiveObject, sky
shader: skybox, reflective
pass: pass1
layer: layer1

 */

struct cameraData_t {
  glm::vec4 clip;//x is near plane, y is far plane, z is cot(fov/2), w is z/aspect
  glm::ivec4 gridOrigin; //xyz is origin sector
  glm::vec4 geometry;
};//transform in separate buffer

constexpr uint32_t skyboxPlaneCount = 8, starTypes = 31;

struct skyboxSeedData_t {
  glm::ivec4 planeIncrement[skyboxPlaneCount];
  glm::ivec4 planeOffset[skyboxPlaneCount];
  glm::vec4 starTypeColors[starTypes];
};

const skyboxSeedData_t skyboxData {
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

constexpr bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr bufferRequirements BRS_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr bufferRequirements BR_skyboxSeedData_skybox = defineSimpleStorageBuffer(gpuId, sizeof(skyboxSeedData_t));
constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BRS_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));

constexpr imageRequirements IR_color_pass1 = {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::RGBA32float,//drivers are REQUIRED by vulkan to support this format for most operations
  .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
  .frameswapCount = 1
}, IR_depth_pass1 = {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::D16unorm,
  .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  .frameswapCount = 1
};

constexpr resourceReference RR_pass1_colorAttachment = defineSimpleColorReference(),
	    RR_pass1_depthAttachment = defineSimpleDepthReference(),
	    RR_skybox_cameraData = defineUBReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RR_skybox_cameraTrans = defineUBReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RR_skybox_skyboxSeedData = defineSBReadonlyReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RR_reflective_sourceTrans = defineUBReferenceAt(vk::ShaderStageFlagBits::eVertex),
	    RR_reflective_cubeRead = defineSamplerReference(),//default stage = fragment
	    RR_reflective_cameraData = defineUBReferenceAt(vk::ShaderStageFlagBits::eVertex),
	    RR_reflective_cameraTrans = defineUBReferenceAt(vk::ShaderStageFlagBits::eVertex),
	    RRL_skybox_targets[] = { RR_skybox_cameraData, RR_skybox_cameraTrans },
	    RRL_reflective_sources[] = { RR_reflective_sourceTrans, RR_reflective_cubeRead },
	    RRL_reflective_targets[] = { RR_reflective_cameraData, RR_reflective_cameraTrans };

constexpr copyStep C_cameraData = defineCopy(),
	    C_cameraTrans = defineCopy(),
	    C_sourceTrans = defineCopy();

constexpr uint64_t RR_IDL_cameraData_common[] = { RR_skybox_cameraData.id, RR_reflective_cameraData.id },
	    RR_IDL_cameraTrans_common[] = { RR_skybox_cameraTrans.id, RR_reflective_cameraTrans.id },
	    RR_IDL_reflective_sourceTrans[] = { RR_reflective_sourceTrans.id, C_sourceTrans.dst.id };

constexpr auto RR_IDL_finalOutput_cameraData = concat<uint64_t, RR_IDL_cameraData_common, C_cameraData.dst.id>(),
	    RR_IDL_finalOutput_cameraTrans = concat<uint64_t, RR_IDL_cameraTrans_common, C_cameraTrans.dst.id>();

constexpr resourceMap RMS_reflectiveObject_transform_staging = {
  .id = __LINE__,
  .requirementId = BRS_singleTransform.id,
  .resourceReferences = C_sourceTrans.src.id,
}, RMS_reflectiveObject_transform = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .resourceReferences = RR_IDL_reflective_sourceTrans,
}, RMLS_reflectiveObject[] = {
  RMS_reflectiveObject_transform_staging,
  RMS_reflectiveObject_transform,
};

static constexpr cubeFactoryData cfd_reflectiveObject {
  .gpuId = gpuId,
  .id_seed = __LINE__*1000,
  .RR_rpColor = RR_pass1_colorAttachment,
  .RR_rpDepth = RR_pass1_depthAttachment,
  .RR_shaderTargetGlobalData = RR_IDL_cameraData_common,
  .RR_shaderTargetPerSideData = RR_IDL_cameraTrans_common,
  .RR_shaderSourceCubeSamplers = RR_reflective_cubeRead.id,
  .format = Format::RGB32float,
  .otherSourceResources = RMLS_reflectiveObject,
};
typedef cubeFactory<cfd_reflectiveObject, cameraData_t> cf_reflectiveObject;

constexpr copyStep C_all[] = {
  C_sourceTrans,
  C_cameraTrans,
  C_cameraData,
  cf_reflectiveObject::C_updateTargetData,
};

constexpr uint64_t C_IDL_L1[] = {
  C_sourceTrans.id,
  C_cameraTrans.id,
  C_cameraData.id,
  cf_reflectiveObject::C_updateTargetData.id,
};

constexpr resourceMap RMT_finalOutput_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BRS_cameraData.id,
  .resourceReferences = C_cameraData.src.id
}, RMT_finalOutput_transform_staging = {
  .id = __LINE__,
  .requirementId = BRS_singleTransform.id,
  .resourceReferences = C_cameraTrans.src.id
}, RMT_finalOutput_color = {
  .id = __LINE__,
  .requirementId = IR_color_pass1.id,
  .resourceReferences = RR_pass1_colorAttachment.id,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true },
}, RMT_finalOutput_depth = {
  .id = __LINE__,
  .requirementId = IR_depth_pass1.id,
  .resourceReferences = RR_pass1_depthAttachment.id,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true },
}, RMLT_finalOutput[] = {
  RMT_finalOutput_cameraData_staging,
  RMT_finalOutput_transform_staging,
  RMT_finalOutput_color,
  RMT_finalOutput_depth,
  {
    .id = __LINE__,
    .requirementId = BR_cameraData.id,
    .resourceReferences = RR_IDL_finalOutput_cameraData,
  },
  {
    .id = __LINE__,
    .requirementId = BR_singleTransform.id,
    .resourceReferences = RR_IDL_finalOutput_cameraTrans,
  }
}, RMS_sky_skyboxSeedData = {
  .id = __LINE__,
  .requirementId = BR_skyboxSeedData_skybox.id,
  .resourceReferences = RR_skybox_skyboxSeedData.id,
  .external = true
};

constexpr sourceLayout SL_sky {
  .id = __LINE__,
  .resources = RMS_sky_skyboxSeedData,
};

constexpr copyableArray<sourceLayout, 2> SLL_all { SL_sky, cf_reflectiveObject::SL };

constexpr copyableArray<uint64_t, SLL_all.LENGTH> SL_IDL_all = [](size_t i){ return SLL_all[i].id; };

constexpr targetLayout TL_finalOutput {
  .id = __LINE__,
  .resources = RMLT_finalOutput,
  .presentImageResourceMapId = RMT_finalOutput_color.id
};

constexpr float spaceDepth = 1;

constexpr vk::SpecializationMapEntry spaceDepthMap { 0, 0, 4 };

defineShaderModules(S_skybox_modules,
		    {
		      .data = vertexWholeScreen_vert,
		      .size = sizeof(vertexWholeScreen_vert),
		      .stage = vk::ShaderStageFlagBits::eVertex,
		      .specializations = spaceDepthMap,
		      .specializationData = (void*)&spaceDepth,
		      .specializationDataSize = sizeof(spaceDepth)
		    },
		    { fragmentSpace_frag, sizeof(fragmentSpace_frag), vk::ShaderStageFlagBits::eFragment });

constexpr graphicsShaderRequirements S_skybox {
  .id = __LINE__,
  .modules = S_skybox_modules,
  .targetProvidedResources = RRL_skybox_targets,
  .sourceProvidedResources = RR_skybox_skyboxSeedData,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3,
};

defineShaderModules(S_reflective_modules,
		    { box_normal_vert, sizeof(box_normal_vert), vk::ShaderStageFlagBits::eVertex },
		    { reflective_frag, sizeof(reflective_frag), vk::ShaderStageFlagBits::eFragment });

constexpr graphicsShaderRequirements S_reflective {
  .id = __LINE__,
  .modules = S_reflective_modules,
  .targetProvidedResources = RRL_reflective_targets,
  .sourceProvidedResources = RRL_reflective_sources,
  .cullMode = vk::CullModeFlagBits::eBack,
  .vertexCountOverride = 36,
};

constexpr graphicsShaderRequirements SL_pass1[] { S_skybox, S_reflective };

constexpr renderPassRequirements RP_pass1 {
  .id = __LINE__,
  .depth = RR_pass1_depthAttachment,
  .color = RR_pass1_colorAttachment,
  .clearDepth = true,
  .clearColor = true,
  .clearColorValue = { 0.0f, 0.0f, 0.0f, 1.0f },
  .shaders = SL_pass1,
};

constexpr renderPassRequirements allRPs[] { RP_pass1 };

constexpr auto TLL_all = concat<targetLayout, cf_reflectiveObject::TL_sides, TL_finalOutput>();

constexpr copyableArray<uint64_t, TLL_all.LENGTH> TL_IDL_all = [](size_t i){ return TLL_all[i].id; };

constexpr layerRequirements L_1 {
  .sourceLayouts = SL_IDL_all,
  .targetLayouts = TL_IDL_all,
  .copies = C_IDL_L1,
  .renders = RP_pass1.id
};

constexpr layerRequirements allLayers[] { L_1 };

constexpr imageRequirements allImageRequirements[] = {
  IR_color_pass1, IR_depth_pass1,
  cf_reflectiveObject::IR_cubeColor, cf_reflectiveObject::IR_depth,
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_singleTransform,
  BRS_singleTransform,
  BR_skyboxSeedData_skybox,
  BR_cameraData,
  BRS_cameraData,
  cf_reflectiveObject::BR_targetData,
  cf_reflectiveObject::BRS_targetData,
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .RPRS = allRPs,
  .CSS = C_all,
  .LRS = allLayers,
  .TLS = TLL_all,
  .SLS = SLL_all,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

typedef cf_reflectiveObject::cube_t<od> reflectiveObject;

constexpr float fov = 45.0f;

int main(int argc, char** argv) {
  gpu::setOptions(argc, argv);
  gpu::init("Reflection test");
  primaryOnion = std::make_unique<onion_t>();
  buffer<BR_skyboxSeedData_skybox> skyboxDataBuf;
  skyboxDataBuf.slowOutOfBandSet(skyboxData);
  auto sky = primaryOnion->createSource<SL_sky.id>();
  sky->set<RMS_sky_skyboxSeedData.id>(&skyboxDataBuf);
  auto camera = primaryOnion->createTarget<TL_finalOutput.id>();
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cd { {}, { 1<<10, 1<<16, 1<<20, 0 }, { 0, glm::tan(glm::radians(fov)/size.y), 0.25f, 0 } };
  cd.clip.x = 0.1f;
  cd.clip.y = 100;
  cd.clip.z = glm::cot(glm::radians(fov/2));
  cd.clip.w = cd.clip.z * size.y / size.x;
  glm::mat4 cameraTransform = glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
  reflectiveObject ro;
  cf_reflectiveObject::create(*primaryOnion, &ro);
  for(size_t i = 0;i < 10000;i++) {
    camera->write<RMT_finalOutput_cameraData_staging.id>(cd);
    camera->write<RMT_finalOutput_transform_staging.id>(cameraTransform);
    primaryOnion->render();
  }
  WARN("sleeping...");
  WITE::Platform::Thread::sleep(1000);//waiting for render to finish. Should have onion destructor wait the active fence instead.
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
}

