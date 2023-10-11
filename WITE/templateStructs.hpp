#pragma once

#include "wite_vulkan.hpp"

namespace WITE {

  struct shaderVertexBufferSlot {
    uint64_t id;
    vk::Format format;
    //no usage flags: always vert
    uint8_t frameswapCount = 1;
  };

  struct shaderUniformBufferSlot {
    uint64_t id;
    vk::ShaderStageFlags readStages, writeStages;
    uint8_t frameswapCount = 1;
  };

  struct shaderImageSlot {//used for both attachments and descriptors
    uint64_t id;
    vk::Format format;
    vk::ShaderStageFlags readStages, writeStages;//stage flags for sampled and storage only, not attachments
    uint8_t frameswapCount = 1;
  };

  struct shaderTargetLayout {
    LiteralList<shaderVertexBufferSlot> vertexBuffers, indexBuffers;
    LiteralList<shaderUniformBufferSlot> uniformBuffers;
    LiteralList<shaderImageSlot> sampled, attachments;
  };

  struct shaderTargetLinkage {
    uint64_t depthStencil, color;
  };

  struct shaderModule {
    uint32_t* data;
    vk::ShaderStageFlags stages;
  };

  struct shader {
    uint64_t id;
    LiteralList<shaderModule> modules;
    shaderTargetLinkage targetLink;
    LiteralList<shaderVertexBufferSlot> vertexBuffers, indexBuffers;
    LiteralList<shaderUniformBufferSlot> uniformBuffers;
    LiteralList<shaderImageSlot> sampled, attachments;
  };

#define defineShaderModules(NOM, ...) defineLiteralList(shaderModule, NOM, __VA_ARGS__)
#define defineVertexBufferSlots(NOM, ...) defineLiteralList(shaderVertexBufferSlot, NOM, __VA_ARGS__)
#define defineUniformBufferSlots(NOM, ...) defineLiteralList(shaderUniformBufferSlot, NOM, __VA_ARGS__)
#define defineImageSlots(NOM, ...) defineLiteralList(shaderImageSlot, NOM, __VA_ARGS__)

}
