#include "../WITE/WITE.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShader.vert.spv.h"

using namespace WITE;

//TODO store some of these configurations in a canned header

constexpr uint64_t gpuId = 0;

constexpr bufferRequirements BR_singleTransform {
  .deviceId = gpuId,
  .id = __LINE__,
  .size = sizeof(glm::dmat4),
  .frameswapCount = 2,//transforms usually can be updated during execution
  .hostToDevice = false
};

constexpr bufferRequirements BR_cubeMesh {
  .deviceId = gpuId,
  .id = __LINE__,
  .size = sizeofUdm<UDM::RGB32float>() * 36,
  .frameswapCount = 2,//transforms usually can be updated during execution
  .hostToDevice = false
};

constexpr imageFlowStep IFS_clearAndDepth {
  .id = __LINE__,
  .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
  .clearOnAttach = true
};

constexpr imageFlowStep IFS_clearAndColor {
  .id = __LINE__,
  .clearOnAttach = true,
  .clearTo = {
    .color = { 0.2f, 0.2f, 0.2f, 0.2f }
  }
};

constexpr imageFlowStep IFS_present {
  .id = __LINE__,
  .layout = vk::ImageLayout::eTransferSrcOptimal
};

constexpr uint64_t IF_color[] = { IFS_clearAndColor.id, IFS_present.id };

constexpr imageRequirements IR_standardDepth {
  .deviceId = gpuId,
  .id = __LINE__,
  .format = Format::D16unorm,
  .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  .flow = IFS_clearAndDepth.id,
  .frameswapCount = 1
};

constexpr imageRequirements IR_standardColor {
  .id = __LINE__,
  .format = Format::RGB8uint,
  .usage = vk::ImageUsageFlagBits::eColorAttachment,
  .flow = IF_color,
  .frameswapCount = 2
};

constexpr resourceMap
RM_depth = {
  .id = __LINE__,
  .requirementId = IR_standardDepth.id,
  .flowId = IFS_clearAndDepth.id,
  .readStages = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
  .writeStages = vk::PipelineStageFlagBits2::eLateFragmentTests,
  .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead
}, RM_color = {
  .id = __LINE__,
  .requirementId = IR_standardColor.id,
  .flowId = IFS_clearAndColor.id,
  .writeStages = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
  .access = vk::AccessFlagBits2::eColorAttachmentWrite
}, RM_cameraTrans = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .readStages = vk::PipelineStageFlagBits2::eVertexShader,
  .access = vk::AccessFlagBits2::eUniformRead
}, RM_target[] = {
  RM_depth, RM_color, RM_cameraTrans
}, RM_cubeTransform = {
  .id = __LINE__,
  .requirementId = BR_singleTransform.id,
  .readStages = vk::PipelineStageFlagBits2::eVertexShader,
  .access = vk::AccessFlagBits2::eUniformRead
}, RM_cubeMesh = {
  .id = __LINE__,
  .requirementId = BR_cubeMesh.id,
  .readStages = vk::PipelineStageFlagBits2::eVertexShader,
  .access = vk::AccessFlagBits2::eVertexAttributeRead
}, RM_source[] = {
  RM_cubeTrans, RM_cubeMesh
};

constexpr targetLayout standardRenderTargetLayout {
  .targetProvidedResources = RM_target //pass by reference (with extra steps), so prior declaration is necessary
};

constexpr shaderTargetLinkage standardRenderTargetLink {
  .depthStencil = RM_depth.id,
  .color = RM_color.id
};

defineShaderModules(simpleShaderModules,
		    { basicShader_vert, vk::ShaderStageFlagBits::eVertex },
		    { basicShader_frag, vk::ShaderStageFlagBits::eFragment });

constexpr shader simpleShader {
  .id = __LINE__,
  .modules = simpleShaderModules,
  .targetLink = standardRenderTargetLink,
  .sourceProvidedResources = RM_cubeTrans
};

constexpr imageFlowStep allFlows[] = {
  IFS_clearAndDepth,
  IFS_clearAndColor,
  IFS_present
};

constexpr imageRequirements allImageRequirements[] = {
  IR_standardDepth,
  IR_standardColor
};

constexpr bufferRequirements allBufferRequirements[] = {
  BR_singleTransform,
  BR_cubeMesh
};

//takes a literalList of shaders, normally there'd be much more than one
typedef WITE::onion<allFlows, allImageRequirements, allBufferRequirements, simpleShader, standardRenderTargetLayout, __LINE__, gpuId> onion_t;
onion_t primaryOnion;

int main(int argc, char** argv) {
  window w;//default window size is a centered rectangle meant for splash screens and tests
  // auto camera = primaryOnion.createTarget();
  // auto cube = primaryOnion.createSource<simpleShader.id>();
  // buffer<BR_singleTransform> cubeTransBuffer, cameraTransBuffer;
  // cube.setUniformBuffer<RM_cubeTrans.id>(&cubeTransBuffer);
  // camera.setUniformBuffer<RM_cameraTrans.id>(&cameraTransBuffer);
  // buffer<BR_cubeMesh> cubeVerts;
  // cube.setVertexBuffer(&cubeVerts);
  // image<IR_standardColor> cameraColor(w.getSize());
  // camera.setAttachment<RM_color.id>(&cameraColor);
  // ////////////////////////////////////////////////////////////
  // image<IR_standardColor> cameraDepth(w.getSize());
  // camera.setAttachment<RM_depth.id>(&cameraDepth);
  // cubeTransBuffer.set(glm::dmat4(1));//model: diagonal identity
  // //TODO abstract out the below math to a camera object or helper function
  // cameraTransBuffer.set(glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
  // 			glm::perspectiveFov<double>(glm::radians(45.0f), 16.0, 9.0, 0.1, 100.0) * //projection
  // 			glm::lookAt(glm::dvec3(-5, 3, -10), glm::dvec3(0, 0, 0), glm::dvec3(0, -1, 0))); //view
  // cubeVerts.set<BR_cubeMesh.format>({
  //     {{0, 0, 0}}, {{1, 1, 0}}, {{1, 0, 0}}, {{0, 0, 0}}, {{0, 1, 0}}, {{1, 1, 0}},
  //     {{0, 0, 1}}, {{1, 1, 1}}, {{1, 0, 1}}, {{0, 0, 1}}, {{0, 1, 1}}, {{1, 1, 1}},
  //     {{0, 0, 0}}, {{1, 0, 1}}, {{1, 0, 0}}, {{0, 0, 0}}, {{0, 0, 1}}, {{1, 0, 1}},
  //     {{0, 1, 0}}, {{1, 1, 1}}, {{1, 1, 0}}, {{0, 1, 0}}, {{0, 1, 1}}, {{1, 1, 1}},
  //     {{0, 0, 0}}, {{0, 1, 1}}, {{0, 1, 0}}, {{0, 0, 0}}, {{0, 0, 1}}, {{0, 1, 1}},
  //     {{1, 0, 0}}, {{1, 1, 1}}, {{1, 1, 0}}, {{1, 0, 0}}, {{1, 0, 1}}, {{1, 1, 1}}
  //   });
  // camera.render();
  // TODO advance frame
  // w.blit(cameraColor);//goes onto same queue as render so fence handled internally
  // Thread::sleep(5000);
}

#error WIP
