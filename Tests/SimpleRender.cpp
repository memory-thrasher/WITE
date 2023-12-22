#include "../WITE/WITE.hpp"
#include "../WITE/Thread.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShader.vert.spv.h"

using namespace WITE;

//TODO store some of these configurations in a canned header

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

// constexpr copyableArray<udmObject<UDM::RGB32float>, 6*6> cubeMesh = {
//   {{0, 0, 0}}, {{1, 1, 0}}, {{1, 0, 0}}, {{0, 0, 0}}, {{0, 1, 0}}, {{1, 1, 0}},
//   {{0, 0, 1}}, {{1, 1, 1}}, {{1, 0, 1}}, {{0, 0, 1}}, {{0, 1, 1}}, {{1, 1, 1}},
//   {{0, 0, 0}}, {{1, 0, 1}}, {{1, 0, 0}}, {{0, 0, 0}}, {{0, 0, 1}}, {{1, 0, 1}},
//   {{0, 1, 0}}, {{1, 1, 1}}, {{1, 1, 0}}, {{0, 1, 0}}, {{0, 1, 1}}, {{1, 1, 1}},
//   {{0, 0, 0}}, {{0, 1, 1}}, {{0, 1, 0}}, {{0, 0, 0}}, {{0, 0, 1}}, {{0, 1, 1}},
//   {{1, 0, 0}}, {{1, 1, 1}}, {{1, 1, 0}}, {{1, 0, 0}}, {{1, 0, 1}}, {{1, 1, 1}}
// };

// constexpr copyableArray<udmObject<UDM::RGB32float>, 3> cubeMesh = {
//   {{0, 0, 0.5}}, {{100, 100, 0.5}}, {{100, 0, 0.5}}
// };

constexpr copyableArray<udmObject<UDM::RGB32float>, 6*6> cubeMesh = {
    // left face
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
    {{-1, -1, 1}}, {{1, 1, 1}}, {{1, -1, 1}},
};

constexpr bufferRequirements BR_singleTransform {
  .deviceId = gpuId,
  .id = __LINE__,
  .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
  .size = sizeof(glm::dmat4),
  .frameswapCount = 1,//staging is frameswapped, updated transform will blow away this buffer every frame
};

constexpr bufferRequirements BR_cubeMesh {
  .deviceId = gpuId,
  .id = __LINE__,
  .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
  .size = sizeof(cubeMesh),//TODO allow dynamic size for meshes
  .frameswapCount = 1,
};

constexpr bufferRequirements BRS_singleTransform {
  .deviceId = gpuId,
  .id = __LINE__,
  .usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
  .size = sizeof(glm::dmat4),
  .frameswapCount = 2,//transforms usually can be updated during execution
  .hostVisible = true
};

//TODO packed function to push constants or otherwise rarely updated without a persisting staging buffer
// ¿cares about whether the target buffer is mappable by the local driver features?
constexpr bufferRequirements BRS_cubeMesh {
  .deviceId = gpuId,
  .id = __LINE__,
  .usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
  .size = sizeof(cubeMesh),
  .frameswapCount = 1,
  .hostVisible = true
};

constexpr imageRequirements IR_standardDepth {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::D16unorm,//drivers are REQUIRED by vulkan to support this format for depth/stencil operations
  .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  .frameswapCount = 2//TODO put this back to 1
};

constexpr imageRequirements IR_standardColor {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::RGBA8unorm,//drivers are REQUIRED by vulkan to support this format for most operations
  .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
  .frameswapCount = 3//TODO put this back to 1
};

constexpr copyStep C_updateCubeTransforms = {
  .id = __LINE__,
  .src = {
    .id = __LINE__
  },
  .dst = {
    .id = __LINE__
  },
}, C_updateCubeMesh = {
  .id = __LINE__,
  .src = {
    .id = __LINE__
  },
  .dst = {
    .id = __LINE__
  },
}, C_updateCameraTransforms = {
  .id = __LINE__,
  .src = {
    .id = __LINE__
  },
  .dst = {
    .id = __LINE__
  },
}, C_all[] = {
  C_updateCubeTransforms, C_updateCubeMesh, C_updateCameraTransforms
};

constexpr resourceReference
RR_depth = {
  .id = __LINE__,
  .stages = vk::ShaderStageFlagBits::eFragment,
  .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead
}, RR_color = {
  .id = __LINE__,
  .stages = vk::ShaderStageFlagBits::eFragment,
  .access = vk::AccessFlagBits2::eColorAttachmentWrite
}, RR_cameraTrans = {
  .id = __LINE__,
  .stages = vk::ShaderStageFlagBits::eVertex,
  .access = vk::AccessFlagBits2::eUniformRead,
  .usage = { vk::DescriptorType::eUniformBuffer }
}, RR_cubeTrans = {
  .id = __LINE__,
  .stages = vk::ShaderStageFlagBits::eVertex,
  .access = vk::AccessFlagBits2::eUniformRead,
  .usage = { vk::DescriptorType::eUniformBuffer }
}, RR_cubeMesh = {
  .id = __LINE__,
  .stages = vk::ShaderStageFlagBits::eVertex,
  .access = vk::AccessFlagBits2::eVertexAttributeRead,
  .usage = { Format::RGB32float, vk::VertexInputRate::eVertex }
}, RRL_simpleSource[] = {
  RR_cubeTrans, RR_cubeMesh
};

constexpr uint64_t RR_IDL_cubeTrans[] = { RR_cubeTrans.id, C_updateCubeTransforms.dst.id },
  RR_IDL_cubeTransStaging[] = { C_updateCubeTransforms.src.id },
  RR_IDL_cubeMesh[] = { RR_cubeMesh.id, C_updateCubeMesh.dst.id },
  RR_IDL_cubeMeshStaging[] = { C_updateCubeMesh.src.id },
  RR_IDL_cameraTrans[] = { RR_cameraTrans.id, C_updateCameraTransforms.dst.id },
  RR_IDL_cameraTransStaging[] = { C_updateCameraTransforms.src.id },
  C_IDL_L1[] = { C_updateCameraTransforms.id, C_updateCubeMesh.id, C_updateCubeTransforms.id };

constexpr resourceMap RMT_cameraTrans = {//this is actually the staging. The real buffer is below
  .id = __LINE__,
  .requirementId = BRS_singleTransform.id,
  .resourceReferences = RR_IDL_cameraTransStaging
}, RMT_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .resourceReferences = RR_color.id
}, RMT_target[] = {
  RMT_cameraTrans,
  RMT_color,
  {//real transform buffer, does not need it's own name because it's not used externally.
    .id = __LINE__,
    .requirementId = IR_standardDepth.id,
    .resourceReferences = RR_depth.id
  }, {
    .id = __LINE__,
    .requirementId = BR_singleTransform.id,
    .resourceReferences = RR_IDL_cameraTrans
  }};

constexpr resourceMap RMS_cubeTrans = {
  .id = __LINE__,
  .requirementId = BRS_singleTransform.id,
  .resourceReferences = RR_IDL_cubeTransStaging
}, RMS_cubeMesh = {
  .id = __LINE__,
  .requirementId = BRS_cubeMesh.id,
  .resourceReferences = RR_IDL_cubeMeshStaging
}, RMS_cube[] = {{
    .id = __LINE__,
    .requirementId = BR_singleTransform.id,
    .resourceReferences = RR_IDL_cubeTrans
  }, {
    .id = __LINE__,
    .requirementId = BR_cubeMesh.id,
    .resourceReferences = RR_IDL_cubeMesh
  },
  RMS_cubeTrans,
  RMS_cubeMesh
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
  .cullMode = vk::CullModeFlagBits::eNone
};

constexpr renderPassRequirements RP_simple {
  .id = __LINE__,
  .depthStencil = RR_depth,
  .color = RR_color,
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
  BR_singleTransform,
  BR_cubeMesh,
  BRS_singleTransform,
  BRS_cubeMesh
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
  gpu::init("Simple render test");
  primaryOnion = std::make_unique<onion_t>();
  auto camera = primaryOnion->createTarget<TL_standardRender.id>();
  auto cube = primaryOnion->createSource<SL_simple.id>();
  cube->write<RMS_cubeMesh.id>(cubeMesh);
  for(size_t i = 0;i < 500;i++) {
    cube->write<RMS_cubeTrans.id>(glm::dmat4(1));//model: diagonal identity
    //TODO abstract out the below math to a camera object or helper function
    camera->write<RMT_cameraTrans.id>(glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
				      glm::perspectiveFov<double>(glm::radians(45.0f), 16.0, 9.0, 0.1, 100.0) * //projection
				      glm::lookAt(glm::dvec3(-15, 13, -10), glm::dvec3(0, 0, 0), glm::dvec3(0, -1, 0))); //view
    primaryOnion->render();
  }
  WARN("NOTE: sleep here (any validation whining after here is in cleanup)");
  Platform::Thread::sleep(500);
}

