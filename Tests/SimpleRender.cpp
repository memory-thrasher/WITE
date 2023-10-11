#include "../WITE/WITE.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShader.vert.spv.h"

using WITE;

constexpr shaderUniformBufferSlot standardTransformData {
  .id = 0,
  .readStages = vk::ShaderStageFlagBits::eVertex,
  .writeStages = 0,
  .frameswapCount = 2
};

constexpr shaderImageSlot standardDepth {
  .id = 1,
  .format = Format::D16unorm,
  .frameswapCount = 1,
  .clearOnFirstUse = true
};

constexpr shaderImageSlot standardColor {
  .id = 0,
  .format = Format::RGB8uint,
  .frameswapCount = 2,
  .clearOnFirstUse = true
};

constexpr shaderImageSlot standardImagery[2] = { standardDepth, standardColor };

constexpr shaderTargetLayout standardRenderTargetLayout {
  .uniformBuffers = standardTransformData,
  .attachments = standardImagery
  //remaining options are default so could be skipped here but included for sample clarity
  // .sampled = NULL
  // .vertexBuffers = NULL
  // .instanceBuffers = NULL
};

constexpr shaderTargetLinkage standardRenderTargetLink {
  .depthStencil = standardDepth.id,
  .color = standardColor.id
};

constexpr shaderVertexBufferSlot standardVertBufferSlot {
  .id = 0,
  .format = UDM::RGB32float, // UDM can have multiple attributes but we only need position today
  .frameswapCount = 1
};

constexpr int transformId[1] = { standardTransformData.id };

defineShaderModules(simpleShaderModules,
		    { basicShader_vert, vk::ShaderStageFlagBits::eVertex },
		    { basicShader_frag, vk::ShaderStageFlagBits::eFragment });

constexpr shader simpleShader {
  .id = 0,
  .modules = simpleShaderModules,
  .targetLink = standardRenderTargetLink,
  .vertexBuffers = standardVertBufferSlot, //also potentially an array
  .uniformBuffers = standardTransformData
  //not used: instanceBuffer, indexBuffer, indirectBuffer
};

//takes a LiteralList of shaders, normally there'd be much more than one
//second is the target layout
typedef WITE::Export::onion<{ simpleShader }, standardRenderTargetLayout> onion_t;
onion_t onion;

constexpr shaderTargetInstanceLayout standardTarget = {
  .targetType = targetType_t::surface, //other options: 2d, 3d, cube
};

int main(int argc, char** argv) {
  auto camera = onion.createTarget<standardTarget>();
  auto cube = onion.createSource<simpleShader.id>();
  buffer<onion_t.shaderUniformBufferRequirements_v<simpleShader.id, standardTransformData.id>> cubeTransBuffer;
  cube.setUniformBuffer<standardTransformData.id>(&cubeTransBuffer);
  buffer<onion_t.shaderVertexBufferRequirements_v<simpleShader.id, standardVertBufferLayout.id>> cubeVerts;
  cube.setVertexBuffer<standardVertBufferLayout.id>(&cubeVerts);
  buffer<onion_t.targetUniformBufferRequirements_v<standardTarget, standardTransformData.id>> cameraTransBuffer;
  camera.setUniformBuffer<standardTransformData.id>(&cameraTransBuffer);
  window<onion_t.getWindowRequirements<standardColor.id>> w;
  camera.setAttachment<standardColor.id>(&w);
  image<onion_t.targetAttachmentRequirements_v<standardTarget, standardDepth.id>> cameraDepth;
  camera.setAttachment<standardDepth.id>(&cameraDepth);
  cubeTransBuffer.set(glm::mat4d::identity);//TODO find real name for dmat4 identity
  cameraTransBuffer.set(glm::mat4d::identity);//TODO MATH
  cubeVerts.set({
      {0, 0, 0}, {1, 1, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0},
      {0, 0, 1}, {1, 1, 1}, {1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1},
      {0, 0, 0}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}, {0, 0, 1}, {1, 0, 1},
      {0, 1, 0}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1}, {1, 1, 1},
      {0, 0, 0}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1}, {0, 1, 1},
      {1, 0, 0}, {1, 1, 1}, {1, 1, 0}, {1, 0, 0}, {1, 0, 1}, {1, 1, 1}
    });
  w.show();
  camera.render();
  Thread::sleep(5000);
}

