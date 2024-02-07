#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShaderWithCamBuffer.vert.spv.h"
#include "vertexWholeScreen.vert.spv.h"
#include "fragmentSphere.frag.spv.h"

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

wrap_mesh(gpuId, UDM::RGB32float, cubeMesh,
          {{-1, -1, -1}}, {{-1, 1, 1}}, {{-1, -1, 1}},
	  {{-1, 1, 1}}, {{-1, -1, -1}}, {{-1, 1, -1}},
	  // front face
	  {{-1, -1, -1}}, {{1, -1, -1}}, {{1, 1, -1}},
	  {{-1, -1, -1}}, {{1, 1, -1}}, {{-1, 1, -1}},
	  // top face
	  {{-1, -1, -1}}, {{1, -1, 1}}, {{1, -1, -1}},
	  {{-1, -1, -1}}, {{-1, -1, 1}}, {{1, -1, 1}},
	  // bottom face
	  {{-1, 1, -1}}, {{1, 1, 1}}, {{-1, 1, 1}},
	  {{-1, 1, -1}}, {{1, 1, -1}}, {{1, 1, 1}},
	  // right face
	  {{1, 1, -1}}, {{1, -1, 1}}, {{1, 1, 1}},
	  {{1, -1, 1}}, {{1, 1, -1}}, {{1, -1, -1}},
	  // back face
	  {{-1, 1, 1}}, {{1, 1, 1}}, {{-1, -1, 1}},
	  {{-1, -1, 1}}, {{1, 1, 1}}, {{1, -1, 1}});

struct sphereData_t {
  glm::vec4 loc;//xyz = location, w = radius
};

struct cameraData_t {
  glm::vec4 loc, norm, right, up;
  glm::vec4 size;
  glm::mat4 transform;
};

constexpr bufferRequirements BR_sphereData = defineSimpleUniformBuffer(gpuId, sizeof(sphereData_t));
constexpr bufferRequirements BRS_sphereData = NEW_ID(stagingRequirementsFor(BR_sphereData, 2));
constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BRS_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));
constexpr bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr bufferRequirements BRS_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr imageRequirements IR_standardDepth = defineSimpleDepth(gpuId);
constexpr imageRequirements IR_standardColor = defineSimpleColor(gpuId);

constexpr copyStep C_updateCubeTransforms = defineCopy(),
	    C_updateSphereData = defineCopy(),
	    C_updateCameraData = defineCopy(),
	    C_all[] = {
	      C_updateCubeTransforms, C_updateCameraData, C_updateSphereData
	    };

constexpr resourceReference RR_depth = defineSimpleDepthReference(),
	    RR_color = defineSimpleColorReference(),
	    RR_cameraData = defineUBReferenceAtVertex(),
	    RR_cameraDataSphere = defineUBReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RR_sphereData = defineUBReferenceAt(vk::ShaderStageFlagBits::eFragment),
	    RR_cubeTrans = defineUBReferenceAtVertex(),
	    RRL_simpleSource[] = { RR_cubeTrans, cubeMesh.resourceReference_v };

constexpr uint64_t RR_IDL_cubeTrans[] = { RR_cubeTrans.id, C_updateCubeTransforms.dst.id },
	    RR_IDL_sphereData[] = { RR_sphereData.id, C_updateSphereData.dst.id },
	    RR_IDL_cameraData[] = { RR_cameraDataSphere.id, RR_cameraData.id, C_updateCameraData.dst.id },
	    C_IDL_L1[] = { C_updateCameraData.id, C_updateCubeTransforms.id, C_updateSphereData.id };

constexpr resourceMap RMT_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BRS_cameraData.id,
  .resourceReferences = C_updateCameraData.src.id
}, RMT_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .resourceReferences = RR_color.id,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true }
}, RMT_target[] = {
  RMT_cameraData_staging,
  RMT_color,
  {//depth map, does not need it's own name because it's not linked externally.
    .id = __LINE__,
    .requirementId = IR_standardDepth.id,
    .resourceReferences = RR_depth.id,
    .resizeBehavior = { imageResizeType::eDiscard, {}, true }
  }, {
    .id = __LINE__,
    .requirementId = BR_cameraData.id,
    .resourceReferences = RR_IDL_cameraData
  }};

constexpr resourceMap RMS_cubeTrans = {
  .id = __LINE__,
  .requirementId = BRS_singleTransform.id,
  .resourceReferences = C_updateCubeTransforms.src.id
}, RMS_cube[] = {
  {
    .id = __LINE__,
    .requirementId = BR_singleTransform.id,
    .resourceReferences = RR_IDL_cubeTrans
  },
  RMS_cubeTrans,
  cubeMesh.resourceMap_v
}, RMS_sphereData = {
  .id = __LINE__,
  .requirementId = BRS_sphereData.id,
  .resourceReferences = C_updateSphereData.src.id
}, RMS_sphere[] = {
  {
    .id = __LINE__,
    .requirementId = BR_sphereData.id,
    .resourceReferences = RR_IDL_sphereData
  },
  RMS_sphereData
};

constexpr targetLayout TL_standardRender {
  .id = __LINE__,
  .resources = RMT_target, //pass by reference (with extra steps), so prior declaration is necessary
  .presentImageResourceMapId = RMT_color.id
};

defineShaderModules(simpleShaderModules, //yes, size is in bytes even though code is an array of 32-bit ints. If shader code is stored in a file (as opposed to a header in this example), it will have to be TRAILING-zero-padded to 4-byte units.
		    { basicShaderWithCamBuffer_vert, sizeof(basicShaderWithCamBuffer_vert), vk::ShaderStageFlagBits::eVertex },
		    { basicShader_frag, sizeof(basicShader_frag), vk::ShaderStageFlagBits::eFragment });

constexpr graphicsShaderRequirements S_cube {
  .id = __LINE__,
  .modules = simpleShaderModules,
  .targetProvidedResources = RR_cameraData,//target is always layout set 1
  .sourceProvidedResources = RRL_simpleSource,//source is always layout set 0
  .cullMode = vk::CullModeFlagBits::eNone
};

defineShaderModules(S_sphereModules,
		    { vertexWholeScreen_vert, sizeof(vertexWholeScreen_vert), vk::ShaderStageFlagBits::eVertex },
		    { fragmentSphere_frag, sizeof(fragmentSphere_frag), vk::ShaderStageFlagBits::eFragment });

constexpr graphicsShaderRequirements S_sphere {
  .id = __LINE__,
  .modules = S_sphereModules,
  .targetProvidedResources = RR_cameraDataSphere,
  .sourceProvidedResources = RR_sphereData,
  .cullMode = vk::CullModeFlagBits::eNone,
  .vertexCountOverride = 3
};

constexpr graphicsShaderRequirements S_RP_1[] = { S_cube, S_sphere };

constexpr renderPassRequirements RP_1 {
  .id = __LINE__,
  .depth = RR_depth,
  .color = RR_color,
  .clearDepth = true,
  .clearColor = true,
  .clearColorValue = { 0.1f, 0.1f, 0.1f, 1.0f },
  .shaders = S_RP_1
};

constexpr sourceLayout SL_cube = {
  .id = __LINE__,
  .resources = RMS_cube
}, SL_sphere = {
  .id = __LINE__,
  .resources = RMS_sphere
};

constexpr sourceLayout SL_all[] = { SL_cube, SL_sphere };

constexpr uint64_t SL_IDL_L_1[] = { SL_cube.id, SL_sphere.id };

constexpr layerRequirements L_1 {
  .sourceLayouts = SL_IDL_L_1,
  .targetLayouts = TL_standardRender.id,
  .copies = C_IDL_L1,
  .renders = RP_1.id
};

constexpr imageRequirements allImageRequirements[] = {
  IR_standardDepth,
  IR_standardColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  cubeMesh.bufferRequirements_v,
  BR_singleTransform,
  BRS_singleTransform,
  BRS_cameraData,
  BR_cameraData,
  BRS_sphereData,
  BR_sphereData,
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .RPRS = RP_1,
  .CSS = C_all,
  .LRS = L_1,
  .TLS = TL_standardRender,
  .SLS = SL_all,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

const float fov = 45;

int main(int argc, char** argv) {
  gpu::setOptions(argc, argv);
  gpu::init("Compute projection test");
  winput::initInput();
  primaryOnion = std::make_unique<onion_t>();
  auto cubeMeshBuf = cubeMesh.spawnMeshBuffer();//note: unique_ptr
  auto camera = primaryOnion->createTarget<TL_standardRender.id>();
  auto cube1 = primaryOnion->createSource<SL_cube.id>();
  cube1->set<cubeMesh.resourceMap_v.id>(cubeMeshBuf.get());
  auto cube2 = primaryOnion->createSource<SL_cube.id>();
  cube2->set<cubeMesh.resourceMap_v.id>(cubeMeshBuf.get());
  auto cube3 = primaryOnion->createSource<SL_cube.id>();
  cube3->set<cubeMesh.resourceMap_v.id>(cubeMeshBuf.get());
  auto cube4 = primaryOnion->createSource<SL_cube.id>();
  cube4->set<cubeMesh.resourceMap_v.id>(cubeMeshBuf.get());
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cameraData;
  auto sphere1 = primaryOnion->createSource<SL_sphere.id>();
  auto sphere2 = primaryOnion->createSource<SL_sphere.id>();
  auto sphere3 = primaryOnion->createSource<SL_sphere.id>();
  auto sphere4 = primaryOnion->createSource<SL_sphere.id>();
  const float r = 1.3;
  float az = 0, inc = 0, dist = 17;
  cameraData.size.x = size.x;
  cameraData.size.y = size.y;
  cameraData.size.z = glm::cot(glm::radians(fov/2));
  cameraData.size.w = cameraData.size.z * size.y / size.x;
  winput::compositeInputData cid;
  while(!shutdownRequested()) {
    winput::pollInput();
    if(winput::getButton(winput::rmb)) {
      winput::getInput(winput::mouse, cid);
      az += cid.axes[0].delta * 0.01f;
      inc += cid.axes[1].delta * 0.01f;
    }
    winput::getInput(winput::mouseWheel, cid);
    dist -= cid.axes[1].numPositive - cid.axes[1].numNegative;
    // WARN(cid.axes[0].numPositive, " -", cid.axes[0].numNegative);
    cameraData.loc = (glm::vec4(glm::cos(az), 0, glm::sin(az), 0) * glm::cos(inc) + glm::vec4(0, glm::sin(inc), 0, 0)) * dist;
    cameraData.transform = makeCameraProjection(fov, camera->getWindow(), 0.1f, 100.0f, glm::vec3(cameraData.loc), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cameraData.norm = glm::normalize(-cameraData.loc);
    cameraData.right = glm::vec4(glm::normalize(glm::cross(glm::vec3(cameraData.norm.x, cameraData.norm.y, cameraData.norm.z), glm::vec3(0, 1, 0))), 0);
    cameraData.up = glm::vec4(glm::normalize(glm::cross(glm::vec3(cameraData.norm.x, cameraData.norm.y, cameraData.norm.z), glm::vec3(cameraData.right.x, cameraData.right.y, cameraData.right.z))), 0);
    camera->write<RMT_cameraData_staging.id>(cameraData);
    sphere1->write<RMS_sphereData.id>(glm::vec4(0, 0, 0, r));
    sphere2->write<RMS_sphereData.id>(glm::vec4(9, 0, 0, r));
    sphere3->write<RMS_sphereData.id>(glm::vec4(0, 5, 0, r));
    sphere4->write<RMS_sphereData.id>(glm::vec4(-9, -5, 5, r));
    cube1->write<RMS_cubeTrans.id>(glm::dmat4(1));
    cube2->write<RMS_cubeTrans.id>(glm::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 9, 0, 0, 1));
    cube3->write<RMS_cubeTrans.id>(glm::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 5, 0, 1));
    cube4->write<RMS_cubeTrans.id>(glm::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -9, -5, 5, 1));
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
  Platform::Thread::sleep(100);
}

