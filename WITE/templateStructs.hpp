#pragma once

#include "wite_vulkan.hpp"
#include "literalList.hpp"
#include "udm.hpp"

namespace WITE {

  constexpr uint64_t NONE = ~0ULL;

  struct shaderModule {
    const uint32_t* data;
    vk::ShaderStageFlags stages;
  };

#define defineShaderModules(NOM, ...) defineLiteralList(shaderModule, NOM, __VA_ARGS__)

  struct resourceMap {
    uint64_t id, requirementId, flowId;//flowId for images with flows only
    vk::PipelineStageFlags2 readStages, writeStages;
    vk::AccessFlags2 access = {};
    uint8_t frameLatency = 0; //must be < requirement.frameswapCount. Generally 0 is the one being written this frame, 1 is the one that was written last frame.
  };

  struct targetLayout {
    uint64_t id;
    literalList<resourceMap> targetProvidedResources;
  };

  struct shaderTargetLinkage {
    uint64_t depthStencil = NONE, color = NONE, vertexBuffer = NONE, instanceBuffer = NONE;
  };

  struct shader {
    uint64_t id;
    literalList<shaderModule> modules;
    shaderTargetLinkage targetLink;
    literalList<resourceMap> sourceProvidedResources;
  };

  struct imageFlowStep {
    uint64_t id = NONE;
    vk::ImageLayout layout = vk::ImageLayout::eGeneral;
    bool clearOnAttach = false;
    vk::ClearValue clearTo = {{ 1.0f, 0 }};
  };

  struct imageRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage = {};
    literalList<uint64_t> flow;
    size_t flowOverwrapIdx = 0;//when advancing past the final flow, which flow becomes current? (so the first N can be skipped, i.e. preinitialized)
    uint8_t dimensions = 2, frameswapCount = 1;
    bool isCube = false, hostVisible = false;
    uint32_t arrayLayers = 1, mipLevels = 1;
  };

  struct bufferRequirements {
    uint64_t deviceId = NONE;
    uint64_t id = NONE;
    udm format;//for vertex buffers only (for now, maybe texel later)
    vk::BufferUsageFlags usage;
    uint32_t size = 0;
    uint8_t frameswapCount = 0;
    bool hostVisible = false;
    //MAYBE isTexel
  };

}

