#include "../WITE/WITE.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "Tests/basicShader.frag.spv.h"
#include "Tests/box_normal.vert.spv.h"
#include "Tests/vertexWholeScreen.vert.spv.h"
#include "Tests/fragmentSphere.frag.spv.h"

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
	    RC_ID_RP_1_depth = __LINE__,
	    RC_ID_RP_1_color = __LINE__;

//end shared
//begin cube
constexpr objectLayout OL_cube = { .id = __LINE__ };

constexpr resourceConsumer RC_S_cube_cameraTrans = defineUBConsumer(Vertex),
	    RC_S_cube_cameraData = defineUBConsumer(Vertex),
	    RC_S_cube_modelTrans = defineUBConsumer(Vertex);

constexpr resourceConsumer RC_L_S_cube_target[] = {
  RC_S_cube_cameraData,
  RC_S_cube_cameraTrans,
};

constexpr resourceConsumer RC_L_S_cube_source[] = {
  RC_S_cube_modelTrans,
};

constexpr shaderModule SM_L_cube[] = {
  { box_normal_vert, sizeof(box_normal_vert), vk::ShaderStageFlagBits::eVertex },
  { basicShader_frag, sizeof(basicShader_frag), vk::ShaderStageFlagBits::eFragment }
};

constexpr graphicsShaderRequirements S_cube {
  .id = __LINE__,
  .modules = SM_L_cube,
  .targetProvidedResources = RC_L_S_cube_target,//target is always layout set 1
  .sourceProvidedResources = RC_L_S_cube_source,//source is always layout set 0
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 36,
};

constexpr resourceSlot RS_cube_trans_staging = {//begin cube
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_cube.id,
}, RS_cube_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_cube.id,
};

constexpr resourceReference RR_L_cube[] = {
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_cube_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_cube_trans.id },
  { .resourceConsumerId = RC_S_cube_modelTrans.id, .resourceSlotId = RS_cube_trans.id },
};

constexpr sourceLayout SL_cube = {
  .id = __LINE__,
  .objectLayoutId = OL_cube.id,
  .resources = RR_L_cube,
};

//end cube
//begin sphere
constexpr objectLayout OL_sphere = { .id = __LINE__ };

struct sphereData_t {
  glm::vec4 loc;//xyz = location, w = radius
};

constexpr bufferRequirements BR_sphereData = defineSimpleUniformBuffer(gpuId, sizeof(sphereData_t));
constexpr bufferRequirements BR_S_sphereData = NEW_ID(stagingRequirementsFor(BR_sphereData, 2));

constexpr resourceConsumer RC_S_sphere_cameraTrans = defineUBConsumer(Fragment),
	    RC_S_sphere_cameraData = defineUBConsumer(Fragment),
	    RC_S_sphere_modelData = defineUBConsumer(Fragment);

constexpr resourceConsumer RC_L_S_sphere_target[] = {
  RC_S_sphere_cameraData,
  RC_S_sphere_cameraTrans,
};

constexpr resourceConsumer RC_L_S_sphere_source[] = {
  RC_S_sphere_modelData,
};

constexpr shaderModule SM_L_sphere[] = {
  { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
  { fragmentSphere_frag, sizeof(fragmentSphere_frag), vk::ShaderStageFlagBits::eFragment }
};

constexpr graphicsShaderRequirements S_sphere {
  .id = __LINE__,
  .modules = SM_L_sphere,
  .targetProvidedResources = RC_L_S_sphere_target,
  .sourceProvidedResources = RC_L_S_sphere_source,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3
};

constexpr resourceSlot RS_sphere_sphereData_staging = {
  .id = __LINE__,
  .requirementId = BR_S_sphereData.id,
  .objectLayoutId = OL_sphere.id,
}, RS_sphere_sphereData = {
  .id = __LINE__,
  .requirementId = BR_sphereData.id,
  .objectLayoutId = OL_sphere.id,
};

constexpr resourceReference RR_L_sphere[] = {
  //lazily reusing transform copy for sphere data that is not really a transform
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_sphere_sphereData_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_sphere_sphereData.id },
  { .resourceConsumerId = RC_S_sphere_modelData.id, .resourceSlotId = RS_sphere_sphereData.id },
};

constexpr sourceLayout SL_sphere = {
  .id = __LINE__,
  .objectLayoutId = OL_sphere.id,
  .resources = RR_L_sphere,
};

//end sphere
//begin camera
struct cameraData_t {
  glm::vec4 clip;//(near plane, far plane, cot(fov/2), z/aspect
};

constexpr objectLayout OL_camera = {
  .id = __LINE__,
  .windowConsumerId = RC_ID_present_color,//this implicitly creates a window that presents the image here by RR
};

constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BR_S_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));
constexpr imageRequirements IR_standardDepth = defineSimpleDepth(gpuId);
constexpr imageRequirements IR_standardColor = defineSimpleColor(gpuId);

constexpr copyStep CP_camera_cameraData = defineCopy();

constexpr resourceSlot RS_camera_cameraData_staging = {
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
  .resizeBehavior = resize_trackWindow_discard,
}, RS_camera_depth = {
  .id = __LINE__,
  .requirementId = IR_standardDepth.id,
  .objectLayoutId = OL_camera.id,
  .resizeBehavior = resize_trackWindow_discard,
}, RS_camera_trans_staging = {
  .id = __LINE__,
  .requirementId = BR_S_singleTransform.id,
  .objectLayoutId = OL_camera.id,
}, RS_camera_trans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .objectLayoutId = OL_camera.id,
};

constexpr resourceReference RR_L_camera[] = {
  { .resourceConsumerId = CP_camera_cameraData.src, .resourceSlotId = RS_camera_cameraData_staging.id },
  { .resourceConsumerId = CP_camera_cameraData.dst, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = CP_transform.src, .resourceSlotId = RS_camera_trans_staging.id },
  { .resourceConsumerId = CP_transform.dst, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_S_cube_cameraData.id, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = RC_S_cube_cameraTrans.id, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_S_sphere_cameraData.id, .resourceSlotId = RS_camera_cameraData.id },
  { .resourceConsumerId = RC_S_sphere_cameraTrans.id, .resourceSlotId = RS_camera_trans.id },
  { .resourceConsumerId = RC_ID_RP_1_color, .resourceSlotId = RS_camera_color.id },
  { .resourceConsumerId = RC_ID_present_color, .resourceSlotId = RS_camera_color.id },
  { .resourceConsumerId = RC_ID_RP_1_depth, .resourceSlotId = RS_camera_depth.id },
};

constexpr targetLayout TL_camera = {
  .id = __LINE__,
  .objectLayoutId = OL_camera.id,
  .resources = RR_L_camera, //pass by reference (with extra steps), so prior declaration is necessary
};

//end camera
//begin layer
constexpr graphicsShaderRequirements S_RP_1[] = { S_cube, S_sphere };

constexpr renderPassRequirements RP_1 {
  .id = __LINE__,
  .depth = RC_ID_RP_1_depth,
  .color = RC_ID_RP_1_color,
  .clearDepth = true,
  .clearColor = true,
  //default depth clear is to 1.0f, could be set here
  .clearColorValue = { 0.1f, 0.1f, 0.1f, 1.0f },
  .shaders = S_RP_1
};

constexpr uint64_t CP_ID_L_L1[] = { CP_transform.id, CP_camera_cameraData.id };

constexpr layerRequirements L_1 {
  .copies = CP_ID_L_L1,
  .renders = RP_1.id,
};

//end layer
//begin OD
constexpr copyStep CP_all[] = {
  CP_camera_cameraData,
  CP_transform,
};

constexpr sourceLayout SL_all[] = {
  SL_sphere,
  SL_cube,
};

constexpr objectLayout OL_L_all[] = {
  OL_sphere,
  OL_cube,
  OL_camera,
};

constexpr imageRequirements allImageRequirements[] = {
  IR_standardDepth,
  IR_standardColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_singleTransform,
  BR_S_singleTransform,
  BR_S_cameraData,
  BR_cameraData,
  BR_S_sphereData,
  BR_sphereData,
};

constexpr resourceSlot RS_L_all[] = {
  RS_camera_trans,
  RS_camera_trans_staging,
  RS_camera_depth,
  RS_camera_color,
  RS_camera_cameraData,
  RS_camera_cameraData_staging,
  RS_sphere_sphereData,
  RS_sphere_sphereData_staging,
  RS_cube_trans,
  RS_cube_trans_staging,
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .RSS = RS_L_all,
  .RPRS = RP_1,
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

int main(int argc, char** argv) {
  configuration::setOptions(argc, argv);
  gpu::init("Input and combined procedural and traditional render test");
  winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->create<OL_camera.id>();
  auto cube1 = primaryOnion->create<OL_cube.id>();
  auto cube2 = primaryOnion->create<OL_cube.id>();
  auto cube3 = primaryOnion->create<OL_cube.id>();
  auto cube4 = primaryOnion->create<OL_cube.id>();
  auto sphere1 = primaryOnion->create<OL_sphere.id>();
  auto sphere2 = primaryOnion->create<OL_sphere.id>();
  auto sphere3 = primaryOnion->create<OL_sphere.id>();
  auto sphere4 = primaryOnion->create<OL_sphere.id>();
  cameraData_t cameraData;
  cameraData.clip.x = 0.1f;
  cameraData.clip.y = 100;
  cameraData.clip.z = glm::cot(glm::radians(fov/2));
  const float r = 1.3;
  float az = 1.5f, inc = 0.5f, dist = 12;
  winput::compositeInputData cid;
  while(!shutdownRequested()) {
    winput::pollInput();
    if(winput::getButton(winput::rmb)) {
      winput::getInput(winput::mouse, cid);
      az += cid.axes[0].delta * 0.01f;
      inc += cid.axes[1].delta * 0.01f;
    }
    winput::getInput(winput::mouseWheel, cid);
    dist -= cid.axes[1].numPositive - cid.axes[1].numNegative;//Normal mouse wheel is y-axis. Some mice have an x-axis.
    glm::vec3 loc = (glm::vec3(glm::cos(az), 0, glm::sin(az)) * glm::cos(inc) + glm::vec3(0, glm::sin(inc), 0)) * (float)pow(1.3f, dist);
    glm::vec2 size = camera->getWindow().getVecSize();
    cameraData.clip.w = cameraData.clip.z * size.y / size.x;
    camera->write<RS_camera_trans_staging.id>(glm::lookAt(loc, glm::vec3(0), glm::vec3(0, 1, 0)));
    camera->write<RS_camera_cameraData_staging.id>(cameraData);
    sphere1->write<RS_sphere_sphereData_staging.id>(glm::vec4(0, 0, 0, r));
    sphere2->write<RS_sphere_sphereData_staging.id>(glm::vec4(9, 0, 0, r));
    sphere3->write<RS_sphere_sphereData_staging.id>(glm::vec4(0, 5, 0, r));
    sphere4->write<RS_sphere_sphereData_staging.id>(glm::vec4(-9, -5, 5, r));
    cube1->write<RS_cube_trans_staging.id>(glm::mat4(1));
    cube2->write<RS_cube_trans_staging.id>(glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 9, 0, 0, 1));
    cube3->write<RS_cube_trans_staging.id>(glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 5, 0, 1));
    cube4->write<RS_cube_trans_staging.id>(glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -9, -5, 5, 1));
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
}

