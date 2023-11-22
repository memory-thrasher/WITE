#pragma once

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "udm.hpp"

namespace WITE {

  constexpr uint64_t NONE = ~0;

  struct shaderVertexBufferSlot {
    uint64_t id;
    udm format;
    //no usage flags: always vert
    uint32_t vertCount;
    uint8_t frameswapCount = 1;
  };

  struct shaderUniformBufferSlot {
    uint64_t id;
    vk::PipelineStageFlags2 readStages, writeStages;
    uint32_t size;
    uint8_t frameswapCount = 1;
  };

  enum class imageFlags_e : uint16_t {//bitmask
    eCube = 1,
    e3DIs2DArray = 2,
    eHostVisible = 4
  };//add more as needed
  using imageFlags_t = vk::Flags<imageFlags_e>;//thx to the folks at vk for doing the legwork on a flag template

  struct shaderAttachmentSlot {
    uint64_t id;
    vk::Format format;
  };

  struct shaderImageSlot {
    uint64_t id;
    vk::Format format;
    vk::PipelineStageFlags2 readStages, writeStages;
    uint8_t dimensions = 2, frameswapCount = 1;
    imageFlags_t imageFlags;
    uint32_t arrayLayers = 1, mipLevels = 1;
  };

  struct shaderTargetLayout {
    literalList<shaderVertexBufferSlot> vertexBuffers, instanceBuffers;
    literalList<shaderUniformBufferSlot> uniformBuffers;
    literalList<shaderImageSlot> sampled, attachments;
  };

  struct shaderTargetLinkage {
    shaderAttachmentSlot depthStencil, color;
    uint64_t vertexBuffer = NONE, instanceBuffer = NONE;
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
    static constexpr size_t MAX_FLOW_STEPS = 4;//going beyond this just means using general for everything
    struct flowStep {
      uint64_t onionId,
	shaderId;
      vk::ImageLayout layout;
      vk::PipelineStageFlags2 stages;
      vk::AccessFlags2 access;

      friend consteval bool operator==(const flowStep& l, const flowStep& r) = default;
    };
    uint64_t deviceId = NONE;
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage;
    flowStep flow[MAX_FLOW_STEPS];
    uint8_t flowCount, dimensions, frameswapCount;
    imageFlags_t imageFlags;
    uint32_t arrayLayers, mipLevels;

    friend consteval bool operator==(const imageRequirements& l, const imageRequirements& r) = default;
  };

  struct bufferRequirements {
    uint64_t deviceId = NONE;
    vk::MemoryPropertyFlags memoryFlags;
    vk::BufferUsageFlags usage = {};
    vk::PipelineStageFlags2 readStages = {}, writeStages = {};
    uint32_t size = 0;
    uint8_t frameswapCount = 0;

    friend consteval auto operator<=>(const bufferRequirements& l, const bufferRequirements& r) = default;
  };

  constexpr bufferRequirements hostVisibleBufferRequirements {
    .memoryFlags = vk::MemoryPropertyFlagBits::eHostCoherent
  };

}

