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
    vk::PipelineStageFlagBits2 readStages, writeStages;
    uint8_t frameswapCount = 1;
  };

  typedef uint16_t imageFlags_t;
  enum class imageFlags_e : imageFlags_t {//bitmask
    eCube = 1,
    e3DIs2DArray = 2,
    eHostVisible = 4
  };//add more as needed

  struct shaderImageSlot {//used for both attachments and descriptors
    uint64_t id;
    vk::Format format;
    vk::PipelineStageFlagBits2 readStages, writeStages;//stage flags for sampled and storage only, not attachments
    uint8_t dimensions = 2, frameswapCount = 1;
    imageFlags_t imageFlags;
    uint32_t arrayLayers = 1, mipLevels = 1;//arraylayers is a hint to the image creation but is not a hard slot requirement
  };

  struct shaderTargetLayout {
    LiteralList<shaderVertexBufferSlot> vertexBuffers, instanceBuffers;
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
    shaderTargetLinkage const targetLink;
    //the remaining fields describe data that will be provided by the source (object being drawn)
    shaderVertexBufferSlot const* vertexBuffer;
    shaderVertexBufferSlot const* instanceBuffer;
    LiteralList<shaderUniformBufferSlot> uniformBuffers;
    //TODO indexBuffer
    LiteralList<shaderImageSlot> sampled;
  };

#define defineShaderModules(NOM, ...) defineLiteralList(shaderModule, NOM, __VA_ARGS__)
#define defineVertexBufferSlots(NOM, ...) defineLiteralList(shaderVertexBufferSlot, NOM, __VA_ARGS__)
#define defineUniformBufferSlots(NOM, ...) defineLiteralList(shaderUniformBufferSlot, NOM, __VA_ARGS__)
#define defineImageSlots(NOM, ...) defineLiteralList(shaderImageSlot, NOM, __VA_ARGS__)

  struct imageRequirements {
    struct flowStep {
      uint64_t onionId,
	shaderId;
      vk::ImageLayout layout;
      vk::PipelineStageFlagBits2 stages;
      vk::AccessFlags2 access;
    };
    uint64_t deviceId;
    vk::ImageUsageFlags usage;
    LiteralList<flowStep> flow;
    uint8_t dimensions = 2, frameswapCount = 0;
    imageFlags_t imageFlags;
    uint32_t arrayLayers = 1, mipLevels = 1;
  };

}

