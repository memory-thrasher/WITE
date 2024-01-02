#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "computeSphere.comp.spv.h"

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

//this should match the one in the shader.
struct sphereData_t {
  glm::vec4 loc;//xyz = location, w = radius
};

struct cameraData_t {
  glm::vec4 loc, norm, up, right;
  glm::vec4 size;
};

constexpr bufferRequirements BR_sphereData = defineSimpleUniformBuffer(gpuId, sizeof(sphereData_t));
constexpr bufferRequirements BRS_sphereData = NEW_ID(stagingRequirementsFor(BR_sphereData, 2));

// constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
// constexpr bufferRequirements BRS_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));
constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BRS_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));

//NOTE: standard color is uniform storage compatible for compute shaders. Sometimes they're better than drawing a mesh
constexpr imageRequirements IR_standardColor = defineSimpleColor(gpuId);

//NOTE: compute depth is not the same format as render depth, and there's no way to get data from compute depth into graphics depth, so compute shaders that do something you don't want a graphics pipe to overwrite need to happen after traditional rending. Traditional depth map (D16unorm) can be blit'ed onto the compute depth (R32float)
//NOTE: depth/stencil cannot be copied to by blit or image copy
constexpr imageRequirements IR_computeDepth = defineComputeDepth(gpuId);

constexpr copyStep C_updateSphereData = defineCopy(),
	    C_updateCameraData = defineCopy(),
	    C_all[] = {
	      C_updateSphereData, C_updateCameraData
	    };

constexpr resourceReference RR_depth = defineComputeDepthReference(),
	    RR_color = defineSimpleColorReference(),
	    RR_cameraData = defineUBReferenceAtCompute(),
	    RR_sphereData = defineUBReferenceAtCompute(),
	    RRL_camera[] { RR_cameraData, RR_depth, RR_color };

constexpr uint64_t RR_IDL_sphereData[] = { RR_sphereData.id, C_updateSphereData.dst.id },
	    RR_IDL_cameraData[] = { RR_cameraData.id, C_updateCameraData.dst.id },
	    C_IDL_L1[] = { C_updateCameraData.id, C_updateSphereData.id };

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
    .requirementId = IR_computeDepth.id,
    .resourceReferences = RR_depth.id,
    .resizeBehavior = { imageResizeType::eDiscard, {}, true }
  }, {
    .id = __LINE__,
    .requirementId = BR_cameraData.id,
    .resourceReferences = RR_IDL_cameraData
  }};

constexpr resourceMap RMS_sphereData = {
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

constexpr shaderModule sphereShaderModule { computeSphere_comp, sizeof(computeSphere_comp), vk::ShaderStageFlagBits::eCompute };

constexpr computeShaderRequirements S_sphere {
  .id = __LINE__,
  .module = &sphereShaderModule,
  .targetProvidedResources = RRL_camera,//target is always layout set 1
  .sourceProvidedResources = RR_sphereData,//source is always layout set 0
  .primaryOutputReferenceId = RR_color.id
};

constexpr sourceLayout SL_simple {
  .id = __LINE__,
  .resources = RMS_sphere
};

constexpr layerRequirements L_1 {
  .sourceLayouts = SL_simple.id,
  .targetLayouts = TL_standardRender.id,
  .copies = C_IDL_L1,
  .computeShaders = S_sphere.id
};

constexpr imageRequirements allImageRequirements[] = {
  IR_computeDepth,
  IR_standardColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_sphereData,
  BRS_sphereData,
  BR_cameraData,
  BRS_cameraData
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .CSRS = S_sphere,
  .CSS = C_all,
  .LRS = L_1,
  .TLS = TL_standardRender,
  .SLS = SL_simple,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

int main(int argc, char** argv) {
  gpu::init("Simple render test");
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->createTarget<TL_standardRender.id>();
  auto sphere = primaryOnion->createSource<SL_simple.id>();
  sphereData_t sd { { 0, 0, 0, 1 } };
  glm::vec2 size = camera->getWindow().getVecSize();
  WARN(sizeof(cameraData_t), ", ", size.x, ", ", size.y);
  cameraData_t cd { { 0, 0, -5, 0 }, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { size.x, size.y, glm::radians(45.0f)/size.y, 0 } };
  // WARN(cd.loc.x, ", ", cd.norm.x, ", ", cd.up.x, ", ", cd.right.x, ", ", cd.size.x);
  // memset((void*)&cd, 1, sizeof(cd));
  for(size_t i = 0;i < 10000;i++) {
    sphere->write<RMS_sphereData.id>(sd);
    camera->write<RMT_cameraData_staging.id>(cd);
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
  // Platform::Thread::sleep(500);
}

