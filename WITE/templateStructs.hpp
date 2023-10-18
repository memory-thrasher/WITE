#pragma once

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "UDM.hpp"

namespace WITE {

  struct shaderVertexBufferSlot {
    uint64_t id;
    udm format;
    //no usage flags: always vert
    uint8_t frameswapCount = 1;
  };

  struct shaderUniformBufferSlot {
    uint64_t id;
    vk::PipelineStageFlags2 readStages, writeStages;
    uint8_t frameswapCount = 1;
  };

  enum class imageFlags_e : uint16_t {//bitmask
    eCube = 1,
    e3DIs2DArray = 2,
    eHostVisible = 4
  };//add more as needed
  using imageFlags_t = vk::Flags<imageFlags_e>;//thx to the folks at vk for doing the legwork on a flag template

  struct shaderImageSlot {//used for both attachments and descriptors
    uint64_t id;
    vk::Format format;
    vk::PipelineStageFlags2 readStages, writeStages;//stage flags for sampled and storage only, not attachments
    uint8_t dimensions = 2, frameswapCount = 1;
    imageFlags_t imageFlags;
    uint32_t arrayLayers = 1, mipLevels = 1;//arraylayers is a hint to the image creation but is not a hard slot requirement
  };

  struct shaderTargetLayout {
    literalList<shaderVertexBufferSlot> vertexBuffers, instanceBuffers;
    literalList<shaderUniformBufferSlot> uniformBuffers;
    literalList<shaderImageSlot> sampled, attachments;
  };

  struct shaderTargetLinkage {
    uint64_t depthStencil, color;
  };

  struct shaderModule {
    const uint32_t* data;
    vk::ShaderStageFlags stages;
  };

  struct shader {
    uint64_t id;
    literalList<shaderModule> modules;
    shaderTargetLinkage const targetLink;
    //the remaining fields describe data that will be provided by the source (object being drawn)
    shaderVertexBufferSlot const* vertexBuffer;//pointer only so it can be nullable
    shaderVertexBufferSlot const* instanceBuffer;//pointer only so it can be nullable
    literalList<shaderUniformBufferSlot> uniformBuffers;
    //TODO indexBuffer
    literalList<shaderImageSlot> sampled;
  };

  enum class targetType_t {
    e2D,
    e3D,
    eCube
  };

  struct shaderTargetInstanceLayout {
    targetType_t targetType;
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
      vk::PipelineStageFlags2 stages;
      vk::AccessFlags2 access;
    };
    uint64_t deviceId;
    vk::Format format;
    vk::ImageUsageFlags usage;
    literalList<flowStep> flow;
    uint8_t dimensions, frameswapCount;
    imageFlags_t imageFlags;
    uint32_t arrayLayers, mipLevels;
  };

}

