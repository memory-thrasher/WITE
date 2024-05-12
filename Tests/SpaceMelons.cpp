#include "../WITE/WITE.hpp"

//begin shared
constexpr uint64_t gpuId = 0;
constexpr WITE::bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr WITE::bufferRequirements BR_S_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr WITE::copyStep CP_transform = defineCopy();
constexpr uint64_t RC_ID_present_color = __LINE__,
	    RC_ID_RP_1_color = __LINE__,
	    RC_ID_RP_1_depth = __LINE__;

//begin spacemelon
constexpr WITE::objectLayout OL_spacemelon = { .id = __LINE__ };

struct spacemelonSharedData_t {
  glm::mat4 transform;
};

constexpr WITE::bufferRequirements BR_spacemelonSharedData = defineSimpleUniformBuffer(gpuId, sizeof(spacemelonSharedData_t)),
	    BR_S_spacemelonSharedData = NEW_ID(stagingRequirementsFor(BR_spacemelonSharedData, 2));

constexpr WITE::resourceSlot RS_spacemelonSharedData = {
  .id = __LINE__,
  .requirementId = BR_spacemelonSharedData.id,
  .objectLayoutId = OL_spacemelon.id,
}, RS_S_spacemelonSharedData = {
  .id = __LINE__,
  .requirementId = BR_S_spacemelonSharedData.id,
  .objectLayoutId = OL_spacemelon.id,
}, RS_L_spacemelonShared[] = {
  RS_spacemelonSharedData,
  RS_S_spacemelonSharedData,
};

//begin spacemelon/high detail mesh

// struct spacemelon_highdetail_indirect_t {
//   vk::DrawMeshTasksIndirectCommandEXT task;
// };

// constexpr WITE::bufferRequirements BR_spacemelon_highdetail_indirect = {
//   .deviceId = gpuId,
//   .id = __LINE__,
//   .usage = vk::BufferUsageFlagBits::eTransferDst /*StorageBuffer*/ | vk::BufferUsageFlagBits::eIndirectBuffer,//compute->vertex
//   .size = sizeof(spacemelon_highdetail_indirect_t),
//   .frameswapCount = 1,
//   .hostVisible = false,
// };

// constexpr WITE::resourceSlot RS_spacemelon_highdetail_indirect = {
//   .id = __LINE__,
//   .requirementId = BR_spacemelon_highdetail_indirect.id,
//   .objectLayoutId = OL_spacemelon.id,
// }, RS_L_spacemelon_highdetail[] = {
//   RS_spacemelon_highdetail_indirect,
// };

#include "Tests/spacemelon_highdetail.mesh.spv.h"
#include "Tests/spacemelon_highdetail.frag.spv.h"

constexpr WITE::resourceConsumer //RC_S_spacemelon_highdetail_indirect = defineIndirectMeshConsumer(),
RC_S_spacemelon_highdetail_sharedData = defineUBConsumer(MeshEXT),
	    RC_L_S_spacemelon_highdetail_source[] = {
	      // RC_S_spacemelon_highdetail_indirect,
	      RC_S_spacemelon_highdetail_sharedData
	    };

constexpr WITE::resourceConsumer RC_S_spacemelon_highdetail_cameraTrans = defineUBConsumer(MeshEXT),
	    RC_S_spacemelon_highdetail_cameraData = defineUBConsumer(MeshEXT),
	    RC_L_S_spacemelon_highdetail_target[] = { RC_S_spacemelon_highdetail_cameraData, RC_S_spacemelon_highdetail_cameraTrans };

constexpr WITE::shaderModule SM_L_spacemelon_highdetail[] = {
  { spacemelon_highdetail_mesh, sizeof(spacemelon_highdetail_mesh), vk::ShaderStageFlagBits::eMeshEXT },
  { spacemelon_highdetail_frag, sizeof(spacemelon_highdetail_frag), vk::ShaderStageFlagBits::eFragment },
};

constexpr WITE::graphicsShaderRequirements S_spacemelon_highdetail {
  .id = __LINE__,
  .modules = SM_L_spacemelon_highdetail,
  .targetProvidedResources = RC_L_S_spacemelon_highdetail_target,
  .sourceProvidedResources = RC_L_S_spacemelon_highdetail_source,
  .cullMode = vk::CullModeFlagBits::eNone,
  .meshGroupCountX = 1,
  .meshGroupCountY = 1,
  .meshGroupCountZ = 1,//160: all mesh implementations are gauranteed to support up to 2^22 total and 2^16 in each dimension
};

constexpr WITE::resourceReference RR_L_spacemelon_highdetail[] = {
  // { .resourceConsumerId = RC_S_spacemelon_highdetail_indirect.id, .resourceSlotId = RS_spacemelon_highdetail_indirect.id },
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_S_spacemelonSharedData.id },//TODO move to compute
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_spacemelonSharedData.id },//TODO move to compute
  { RC_S_spacemelon_highdetail_sharedData.id, RS_spacemelonSharedData.id },
};

constexpr WITE::sourceLayout SL_spacemelon_highdetail = {
  .id = __LINE__,
  .objectLayoutId = OL_spacemelon.id,
  .resources = RR_L_spacemelon_highdetail,
};

//begin camera
struct cameraData_t {
  glm::vec4 clip;//(near plane, far plane, cot(fov/2), z/aspect
};

constexpr WITE::objectLayout OL_camera = {
  .id = __LINE__,
  .windowConsumerId = RC_ID_present_color,//this implicitly creates a window that presents the image here by RR
};

constexpr WITE::bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr WITE::bufferRequirements BR_S_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));
constexpr WITE::imageRequirements IR_standardColor = defineSimpleColor(gpuId);
constexpr WITE::imageRequirements IR_standardDepth = defineSimpleDepth(gpuId);

constexpr WITE::copyStep CP_camera_cameraData = defineCopy();

constexpr WITE::resourceSlot RS_camera_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BR_S_cameraData.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_cameraData = {
  .id = __LINE__,
  .requirementId = BR_cameraData.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = WITE::resize_trackWindow_discard,
}, RS_camera_depth = {
  .id = __LINE__,
  .requirementId = IR_standardDepth.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = WITE::resize_trackWindow_discard,
}, RS_camera_trans_staging = {
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_L_camera[] = {
  RS_camera_trans,
  RS_camera_trans_staging,
  RS_camera_color,
  RS_camera_depth,
  RS_camera_cameraData,
  RS_camera_cameraData_staging,
};

constexpr WITE::resourceReference RR_L_camera[] = {
  { .resourceConsumerId = CP_camera_cameraData.src, .resourceSlotId = RS_camera_cameraData_staging.id },
  { .resourceConsumerId = CP_camera_cameraData.dst, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_camera_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_S_spacemelon_highdetail_cameraData.id, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = RC_S_spacemelon_highdetail_cameraTrans.id, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_ID_RP_1_depth, .resourceSlotId = RS_camera_depth.id },
  { .resourceConsumerId = RC_ID_RP_1_color, .resourceSlotId = RS_camera_color.id },
  { .resourceConsumerId = RC_ID_present_color, .resourceSlotId = RS_camera_color.id },
};

constexpr WITE::targetLayout TL_camera = {
  .id = __LINE__,
  .objectLayoutId = OL_camera.id,
  .resources = RR_L_camera,
};

constexpr WITE::graphicsShaderRequirements S_L_RP1[] = {
  S_spacemelon_highdetail,
};

constexpr WITE::renderPassRequirements RP_1 = {
  .id = __LINE__,
  .depth = RC_ID_RP_1_depth,
  .color = RC_ID_RP_1_color,
  .clearDepth = true,
  .clearColor = true,
  .clearColorValue = { 0, 0, 0, 0 },
  .shaders = S_L_RP1,
};

constexpr uint64_t CP_IDL_all[] = { CP_camera_cameraData.id, CP_transform.id };

constexpr WITE::layerRequirements LRS[] = {
  {
    .copies = CP_IDL_all,
    .renders = RP_1.id,
  }
};

//begin onion
constexpr WITE::bufferRequirements allBRS[] = {
  BR_S_cameraData,
  BR_cameraData,
  // BR_spacemelon_highdetail_indirect,
  BR_S_singleTransform,
  BR_singleTransform,
  BR_spacemelonSharedData,
  BR_S_spacemelonSharedData,
};

constexpr auto RS_all = WITE::concat<WITE::resourceSlot, // RS_L_spacemelon_highdetail, 
	    RS_L_spacemelonShared, RS_L_camera>();

constexpr WITE::copyStep CP_all[] = { CP_camera_cameraData, CP_transform };

constexpr WITE::objectLayout allOLS[] = { OL_spacemelon, OL_camera };

constexpr WITE::imageRequirements IR_all[] = { IR_standardColor, IR_standardDepth };

constexpr auto SL_L_all = WITE::concat<WITE::sourceLayout, SL_spacemelon_highdetail>();

constexpr WITE::onionDescriptor od = {
  .IRS = IR_all,
  .BRS = allBRS,
  .RSS = RS_all,
  .RPRS = RP_1,
  .CSS = CP_all,
  .LRS = LRS,
  .OLS = allOLS,
  .TLS = TL_camera,
  .SLS = SL_L_all,
  .GPUID = gpuId,
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

const float fov = 45;

int main(int argc, char** argv) {
  WITE::configuration::setOptions(argc, argv);
  WITE::gpu::init("test", {}, {}, {"VK_EXT_mesh_shader"});
  WITE::winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->create<OL_camera.id>();
  cameraData_t cameraData;
  cameraData.clip.x = 0.1f;
  cameraData.clip.y = 100;
  cameraData.clip.z = glm::cot(glm::radians(fov/2));
  auto spacemelon1 = primaryOnion->create<OL_spacemelon.id>();
  spacemelonSharedData_t spacemelon1data = { glm::translate(glm::mat4(1), glm::vec3(-10, 0, 0)) };
  // spacemelon_highdetail_indirect_t spacemelon_highdetail_indirect { {1, 1, 1} };
  // spacemelon1->get<RS_spacemelon_highdetail_indirect.id>().slowOutOfBandSet(spacemelon_highdetail_indirect);
  float az = 1.5f, inc = 0.5f, dist = 12;
  WITE::winput::compositeInputData cid;
  constexpr float incLimit = ::glm::pi<float>()/2-glm::epsilon<float>();
  while(!WITE::shutdownRequested()) {
  // for(size_t i = 0;i < 10 && !WITE::shutdownRequested();i++) {
    WITE::winput::pollInput();
    if(WITE::winput::getButton(WITE::winput::rmb)) {
      WITE::winput::getInput(WITE::winput::mouse, cid);
      az += cid.axes[0].delta * 0.01f;
      inc += cid.axes[1].delta * 0.01f;
      inc = glm::clamp(inc, -incLimit, incLimit);
    }
    WITE::winput::getInput(WITE::winput::mouseWheel, cid);
    dist -= cid.axes[1].numPositive - cid.axes[1].numNegative;//Normal mouse wheel is y-axis. Some mice have an x-axis.
    glm::vec3 loc = (glm::vec3(glm::cos(az), 0, glm::sin(az)) * glm::cos(inc) + glm::vec3(0, glm::sin(inc), 0)) * (float)pow(1.3f, dist);
    glm::vec2 size = camera->getWindow().getVecSize();
    cameraData.clip.w = cameraData.clip.z * size.y / size.x;
    camera->write<RS_camera_trans_staging.id>(glm::lookAt(loc, glm::vec3(0), glm::vec3(0, 1, 0)));
    camera->write<RS_camera_cameraData_staging.id>(cameraData);
    spacemelon1->write<RS_S_spacemelonSharedData.id>(spacemelon1data);
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
  PROFILE_DUMP;
}
