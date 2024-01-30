#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShader.vert.spv.h"

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

constexpr bufferRequirements BR_singleTransform = defineSingleTransform(gpuId);
constexpr bufferRequirements BRS_singleTransform = NEW_ID(stagingRequirementsFor(BR_singleTransform, 2));
constexpr imageRequirements IR_standardDepth = defineSimpleDepth(gpuId);
constexpr imageRequirements IR_standardColor = defineSimpleColor(gpuId);

constexpr copyStep C_updateCubeTransforms = defineCopy(),
	    C_updateCameraTransforms = defineCopy(),
	    C_all[] = {
	      C_updateCubeTransforms, C_updateCameraTransforms
	    };

constexpr resourceReference RR_depth = defineSimpleDepthReference(),
	    RR_color = defineSimpleColorReference(),
	    RR_cameraTrans = defineUBReferenceAtVertex(),
	    RR_cubeTrans = defineUBReferenceAtVertex(),
	    RRL_simpleSource[] = { RR_cubeTrans, cubeMesh.resourceReference_v };

constexpr uint64_t RR_IDL_cubeTrans[] = { RR_cubeTrans.id, C_updateCubeTransforms.dst.id },
  RR_IDL_cameraTrans[] = { RR_cameraTrans.id, C_updateCameraTransforms.dst.id },
  C_IDL_L1[] = { C_updateCameraTransforms.id, C_updateCubeTransforms.id };

constexpr resourceMap RMT_cameraTrans_staging = {
  .id = __LINE__,
  .requirementId = BRS_singleTransform.id,
  .resourceReferences = C_updateCameraTransforms.src.id
}, RMT_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .resourceReferences = RR_color.id,
  .resizeBehavior = { imageResizeType::eDiscard, {}, true }
}, RMT_target[] = {
  RMT_cameraTrans_staging,
  RMT_color,
  {//depth map, does not need it's own name because it's not linked externally.
    .id = __LINE__,
    .requirementId = IR_standardDepth.id,
    .resourceReferences = RR_depth.id,
    .resizeBehavior = { imageResizeType::eDiscard, {}, true }
  }, {
    .id = __LINE__,
    .requirementId = BR_singleTransform.id,
    .resourceReferences = RR_IDL_cameraTrans
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
};

constexpr targetLayout TL_standardRender {
  .id = __LINE__,
  .resources = RMT_target, //pass by reference (with extra steps), so prior declaration is necessary
  .presentImageResourceMapId = RMT_color.id
};

defineShaderModules(simpleShaderModules, //yes, size is in bytes even though code is an array of 32-bit ints. If shader code is stored in a file (as opposed to a header in this example), it will have to be TRAILING-zero-padded to 4-byte units.
		    { basicShader_vert, sizeof(basicShader_vert), vk::ShaderStageFlagBits::eVertex },
		    { basicShader_frag, sizeof(basicShader_frag), vk::ShaderStageFlagBits::eFragment });

constexpr graphicsShaderRequirements S_simple {
  .id = __LINE__,
  .modules = simpleShaderModules,
  .targetProvidedResources = RR_cameraTrans,//target is always layout set 1
  .sourceProvidedResources = RRL_simpleSource,//source is always layout set 0
  .cullMode = vk::CullModeFlagBits::eBack
};

constexpr renderPassRequirements RP_simple {
  .id = __LINE__,
  .depth = RR_depth,
  .color = RR_color,
  .clearDepth = true,
  .clearColor = true,
  .clearColorValue = { 0.1f, 0.1f, 0.1f, 1.0f },
  .shaders = S_simple//takes a literalList of shaders, normally there'd be much more than one
};

constexpr sourceLayout SL_simple {
  .id = __LINE__,
  .resources = RMS_cube
};

constexpr layerRequirements L_1 {
  .sourceLayouts = SL_simple.id,
  .targetLayouts = TL_standardRender.id,
  .copies = C_IDL_L1,
  .renders = RP_simple.id
};

constexpr imageRequirements allImageRequirements[] = {
  IR_standardDepth,
  IR_standardColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  cubeMesh.bufferRequirements_v,
  BR_singleTransform,
  BRS_singleTransform
};

constexpr onionDescriptor od = {
  .IRS = allImageRequirements,
  .BRS = allBufferRequirements,
  .RPRS = RP_simple,
  .CSS = C_all,
  .LRS = L_1,
  .TLS = TL_standardRender,
  .SLS = SL_simple,
  .GPUID = gpuId
};

typedef WITE::onion<od> onion_t;
std::unique_ptr<onion_t> primaryOnion;

int main(int argc, char** argv) {
  gpu::setOptions(argc-1, argv+1);
  gpu::init("Simple render test");
  primaryOnion = std::make_unique<onion_t>();
  auto cubeMeshBuf = cubeMesh.spawnMeshBuffer();//note: unique_ptr
  auto camera = primaryOnion->createTarget<TL_standardRender.id>();
  auto cube = primaryOnion->createSource<SL_simple.id>();
  cube->set<cubeMesh.resourceMap_v.id>(cubeMeshBuf.get());
  glm::dvec3 rotAxis = glm::normalize(glm::dvec3(0, 1, 0));
  glm::dmat4 model = glm::dmat4(1);//model: diagonal identity
  for(size_t i = 0;i < 10000;i++) {
    model = glm::rotate(model, glm::radians(0.01), rotAxis);
    cube->write<RMS_cubeTrans.id>(model);
    camera->write<RMT_cameraTrans_staging.id>(makeCameraProjection(45, camera->getWindow(), 0.1f, 100.0f, glm::dvec3(-15, 13, -10), glm::dvec3(0, 0, 0), glm::dvec3(0, 1, 0)));
    primaryOnion->render();
  }
  WARN("NOTE: done rendering (any validation whining after here is in cleanup)");
  // Platform::Thread::sleep(500);
}

