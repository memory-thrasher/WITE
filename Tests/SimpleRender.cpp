#include "../WITE/WITE.hpp"

//include the compiled shader code which was outputted as a header in the build dir before c++ code gets compiled.
#include "basicShader.frag.spv.h"
#include "basicShader.vert.spv.h"

using WITE;

//NOTE: all .id fields need only be unique within the onion. __LINE__ is a cute way to do that when all in one file.
//more complex use cases may map multiple resources to the same id to reuse that resource in a different manner within the onion
//obviously some things with ids are not compatible like shaders and buffers

constexpr shaderUniformBufferSlot objectTransformData {
  .id = __LINE__,
  .readStages = vk::ShaderStageFlagBits::eVertex,
  .writeStages = 0,
  .frameswapCount = 2
}, cameraTransformData {
  .id = __LINE__,
  .readStages = vk::ShaderStageFlagBits::eVertex,
  .writeStages = 0,
  .frameswapCount = 2
};

constexpr union {
  struct {
    shaderImageSlot standardDepth {
      .id = __LINE__,
      .format = Format::D16unorm,
      .frameswapCount = 1
    };
    shaderImageSlot standardColor {
      .id = __LINE__,
      .format = Format::RGB8uint,
      .frameswapCount = 2
    };
  };
  shaderImageSlot standardImagery[2];
};

constexpr shaderTargetLayout standardRenderTargetLayout {
  .uniformBuffers = cameraTransformData,
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
  .id = __LINE__,
  .format = UDM::RGB32float, // UDM can have multiple attributes but we only need position today
  .frameswapCount = 1 //static mesh: no need to double-buffer
};

defineShaderModules(simpleShaderModules,
		    { basicShader_vert, vk::ShaderStageFlagBits::eVertex },
		    { basicShader_frag, vk::ShaderStageFlagBits::eFragment });

constexpr shader simpleShader {
  .id = __LINE__,
  .modules = simpleShaderModules,
  .targetLink = standardRenderTargetLink,
  .vertexBuffer = standardVertBufferSlot,
  .uniformBuffers = objectTransformData//could be an array
};

//takes a LiteralList of shaders, normally there'd be much more than one
//second is the target layout
typedef WITE::Export::onion<{ simpleShader }, standardRenderTargetLayout, __LINE__> onion_t;
onion_t onion;

constexpr shaderTargetInstanceLayout standardTarget = {
  .targetType = targetType_t::surface, //other options: 2d, 3d, cube
};

int main(int argc, char** argv) {
  auto camera = onion.createTarget<standardTarget>();
  auto cube = onion.createSource<simpleShader.id>();
  buffer<onion_t::bufferRequirements_v<standardTransformData.id>> cubeTransBuffer;
  cube.setUniformBuffer<standardTransformData.id>(&cubeTransBuffer);
  buffer<onion_t::bufferRequirements_v<standardVertBufferLayout.id>> cubeVerts;
  cube.setVertexBuffer<standardVertBufferLayout.id>(&cubeVerts);
  buffer<onion_t::bufferRequirements_v<standardTarget, standardTransformData.id>> cameraTransBuffer;
  camera.setUniformBuffer<standardTransformData.id>(&cameraTransBuffer);
  window<onion_t::windowRequirements_v<standardColor.id>> w;
  camera.setAttachment<standardColor.id>(&w);
  image<onion_t::imageRequirements_v<standardTarget, standardDepth.id>> cameraDepth;
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

[
