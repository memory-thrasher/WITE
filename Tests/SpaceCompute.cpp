#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "computeSpace.comp.spv.h"

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
};

constexpr bufferRequirements BR_cameraData = defineSimpleUniformBuffer(gpuId, sizeof(cameraData_t));
constexpr bufferRequirements BRS_cameraData = NEW_ID(stagingRequirementsFor(BR_cameraData, 2));

//NOTE: standard color is uniform storage compatible for compute shaders. Sometimes they're better than drawing a mesh
constexpr imageRequirements IR_standardColor = defineSimpleColor(gpuId);

constexpr copyStep C_updateCameraData = defineCopy(),
	    C_all[] = {
	      C_updateCameraData
	    };

constexpr clearStep CL_color = defineClear(0.2f, 0.2f, 0.2f, 1.0f),
	    CL_all[] = { CL_color };

constexpr resourceReference RR_color = defineSimpleColorReference(),
	    RR_cameraData = defineUBReferenceAtCompute(),
	    RRL_camera[] { RR_cameraData, RR_color };

constexpr uint64_t RR_IDL_cameraData[] = { RR_cameraData.id, C_updateCameraData.dst.id },
	    RR_IDL_color[] = { RR_color.id, CL_color.rr.id },
	    C_IDL_L1[] = { C_updateCameraData.id },
	    CL_IDL_L1[] = { CL_color.id };

constexpr resourceMap RMT_cameraData_staging = {
  .id = __LINE__,
  .requirementId = BRS_cameraData.id,
  .resourceReferences = C_updateCameraData.src.id
}, RMT_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .resourceReferences = RR_IDL_color,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true }
}, RMT_target[] = {
  RMT_cameraData_staging,
  RMT_color,
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
  .primaryOutputReferenceId = RR_color.id
};

constexpr layerRequirements L_1 {
  .targetLayouts = TL_standardRender.id,
  .clears = CL_IDL_L1,
  .copies = C_IDL_L1,
  .computeShaders = S_space.id
};

constexpr imageRequirements allImageRequirements[] = {
  IR_standardColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_cameraData,
  BRS_cameraData
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .CSRS = S_space,
  .CLS = CL_all,
  .CSS = C_all,
  .LRS = L_1,
  .TLS = TL_standardRender,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

int main(int argc, char** argv) {
  gpu::init("Space test");
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->createTarget<TL_standardRender.id>();
  glm::vec2 size = camera->getWindow().getVecSize();
  cameraData_t cd { { 0, 0, -5, 0 }, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }, { 1, 0, 0, 0 }, { size.x, size.y, glm::radians(45.0f)/size.y, 0 } };
  for(size_t i = 0;i < 100;i++) {
    cd.norm.x = std::sin(i/1000.0f);
    cd.norm.z = std::cos(i/1000.0f);
    cd.loc = cd.norm * -5.0f;
    cd.right = glm::vec4(glm::cross(glm::vec3(cd.up), glm::vec3(cd.norm)), 0);
    camera->write<RMT_cameraData_staging.id>(cd);
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
  Platform::Thread::sleep(5000);
}

