#pragma once

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "udm.hpp"

namespace WITE {

  constexpr uint64_t NONE = ~0ULL;

//   struct shaderVertexBufferSlot {
//     uint64_t resourceId;
//     udm format;
//   };

//   struct shaderUniformBufferSlot {
//     uint64_t resourceId;
//     vk::PipelineStageFlags2 readStages, writeStages;
//   };

//   enum class imageFlags_e : uint16_t {//bitmask
//     eCube = 1,
//     e3DIs2DArray = 2,
//     eHostVisible = 4
//   };//add more as needed
//   using imageFlags_t = vk::Flags<imageFlags_e>;//thx to the folks at vk for doing the legwork on a flag template

//   struct shaderAttachmentSlot {
//     uint64_t id;
//   };

//   struct shaderImageSlot {
//     uint64_t id;
//     vk::PipelineStageFlags2 readStages, writeStages;
//   };

//   struct shaderTargetLayout {
//     literalList<shaderVertexBufferSlot> vertexBuffers, instanceBuffers;
//     literalList<shaderUniformBufferSlot> uniformBuffers;
//     literalList<shaderImageSlot> sampled, attachments;
//   };

//   struct shaderTargetLinkage {
//     shaderAttachmentSlot depthStencil, color;
//     uint64_t vertexBuffer = NONE, instanceBuffer = NONE;
//   };

//   struct shaderModule {
//     const uint32_t* data;
//     vk::ShaderStageFlags stages;
//   };

//   struct shader {
//     uint64_t id;
//     literalList<shaderModule> modules;
//     shaderTargetLinkage const targetLink;
//     //the remaining fields describe data that will be provided by the source (object being drawn)
//     shaderVertexBufferSlot const* vertexBuffer;//pointer only so it can be nullable
//     shaderVertexBufferSlot const* instanceBuffer;//pointer only so it can be nullable
//     literalList<shaderUniformBufferSlot> uniformBuffers;
//     //TODO indexBuffer
//     literalList<shaderImageSlot> sampled;
//   };

//   enum class targetType_t {
//     e2D,
//     e3D,
//     eCube
//   };

//   struct shaderTargetInstanceLayout {
//     targetType_t targetType;
//   };

// #define defineVertexBufferSlots(NOM, ...) defineLiteralList(shaderVertexBufferSlot, NOM, __VA_ARGS__)
// #define defineUniformBufferSlots(NOM, ...) defineLiteralList(shaderUniformBufferSlot, NOM, __VA_ARGS__)
// #define defineImageSlots(NOM, ...) defineLiteralList(shaderImageSlot, NOM, __VA_ARGS__)

  struct shaderModule {
    const uint32_t* data;
    vk::ShaderStageFlags stages;
  };

#define defineShaderModules(NOM, ...) defineLiteralList(shaderModule, NOM, __VA_ARGS__)

  struct resourceMap {
    uint64_t id, requirementId, flowId;//flowId for images with flows only
    vk::PipelineStageFlags2 readStages, writeStages;
    vk::AccessFlags2 access = {};
  };

  struct targetLayout {
    literalList<resourceMap> targetProvidedResources;
  };

  struct shaderTargetLinkage {
    uint64_t depthStencil = NONE, color = NONE, vertexBuffer = NONE, instanceBuffer = NONE;
  };

  struct shader {
    uint64_t id;
    literalList<shaderModule> modules;
    shaderTargetLinkage targetLink;
    literalList<resourceMap> shaderProvidedResources;
  };

  struct imageFlowStep {
    uint64_t id = NONE;
    vk::ImageLayout layout = vk::ImageLayout::eGeneral;
    //remaining are not needed if flowCount = 1 (bc no transitions will happen)
    bool clearOnAttach = false;
    vk::ClearValue clearTo = {{ 1.0f, 0 }};
  };

  struct imageRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage = {};
    literalList<uint64_t> flow;
    uint8_t dimensions = 2, frameswapCount = 2;
    bool isCube = false, hostToDevice = false, deviceToHost = false;
    uint32_t arrayLayers = 1, mipLevels = 1;
  };

  struct bufferRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;
    udm format;//for vertex buffers only (for now, maybe texel later)
    vk::BufferUsageFlags usage;
    uint32_t size = 0;
    uint8_t frameswapCount = 0;
    //hostToDevice, deviceToHost control usage of staging buffer
    bool hostToDevice = false, deviceToHost = false;
    //MAYBE isTexel
  };

}

