#pragma once

#include "wite_vulkan.hpp"
#include "templateStructs.hpp"
#include "literalList.hpp"
#include "buffer.hpp"

namespace WITE {

#define wrap_mesh(GPU, U, NOM, ...) constexpr meshWrapper< GPU, U, ::WITE::countIL<udmObject<U>>({ __VA_ARGS__ }), __LINE__ * 1000000 > NOM = { __VA_ARGS__ }

  //produces configuration structs with id values of [ID, ID+2]
  template<size_t gpuId, udm U, size_t C, uint64_t ID, bool instance = false> struct meshWrapper {

    static constexpr uint64_t id = ID;

    static constexpr bufferRequirements bufferRequirements_v {
      .deviceId = gpuId,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
      .size = sizeofUdm<U>() * C,
      .frameswapCount = 1,
    };

    static constexpr resourceReference resourceReference_v = {
      .id = ID+1,
      .stages = vk::ShaderStageFlagBits::eVertex,
      .access = vk::AccessFlagBits2::eVertexAttributeRead,
      .usage = { U, instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex }
    };

    static constexpr resourceMap resourceMap_v = {
      .id = ID+2,
      .requirementId = bufferRequirements_v.id,
      .resourceReferences = resourceReference_v.id,
      .external = true
    };

    typedef buffer<bufferRequirements_v> buffer_t;

    copyableArray<udmObject<U>, C> mesh;

    constexpr meshWrapper(std::initializer_list<udmObject<U>> il) : mesh(il) {};

    void load(buffer_t* b) const {
      b->slowOutOfBandSet(mesh);
    };

    std::unique_ptr<buffer_t> spawnMeshBuffer() const {
      auto ret = std::make_unique<buffer_t>();
      load(ret.get());
      return ret;
    };

  };

#define defineSimpleUniformBuffer(gpuId, size) simpleUB<gpuId, __LINE__, size>::value

  template<size_t GPUID, uint64_t ID, uint32_t size> struct simpleUB {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
      .size = size,
      .frameswapCount = 1,//staging should be frameswapped, updated data will blow away this buffer every frame
    };
  };

#define defineSimpleStorageBuffer(gpuId, size) simpleSB<gpuId, __LINE__, size>::value

  template<size_t GPUID, uint64_t ID, uint32_t size> struct simpleSB {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
      .size = size,
      .frameswapCount = 1,
    };
  };

#define defineSingleTransform(gpuId) singleTransform<gpuId, __LINE__>::value

  template<size_t GPUID, uint64_t ID> struct singleTransform {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
      .size = sizeof(glm::dmat4),
      .frameswapCount = 1,//staging should be frameswapped, updated transform will blow away this buffer every frame
    };
  };

#define defineComputeDepth(gpuId) computeDepth<gpuId, __LINE__>::value

  template<size_t GPUID, uint64_t ID> struct computeDepth {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = Format::R32float,//drivers are REQUIRED by vulkan to support this format for storage and blit ops
      //MAYBE switch to R32uint in the future because it has atomic BUT complicates blit
      .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst,//transfer dst is needed for clear and for copying from a traditional depth map, one of those should happen
      .frameswapCount = 1
    };
  };

#define defineSimpleDepth(gpuId) simpleDepth<gpuId, __LINE__>::value

  template<size_t GPUID, uint64_t ID> struct simpleDepth {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = Format::D16unorm,//drivers are REQUIRED by vulkan to support this format for depth operations (and blit src)
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .frameswapCount = 1
    };
  };

#define defineSimpleColor(gpuId) simpleColor<gpuId, __LINE__>::value

  template<size_t GPUID, uint64_t ID> struct simpleColor {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = Format::RGBA8unorm,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,//transfer src is needed by window to present, transfer dst to clear (unless on a RP)
      .frameswapCount = 2
    };
  };

#define defineCopy() simpleCopy<__LINE__ * 1000000>::value
  //yeah there's not much to this, just a crosswalk
  template<uint64_t ID> struct simpleCopy {
    static constexpr copyStep value {
      .id = ID,
      .src = {
	.id = ID+1
      },
      .dst = {
	.id = ID+2
      },
    };
  };

#define defineClear(...) simpleClear<vk::ClearValue {{ __VA_ARGS__ }}, __LINE__ * 1000000>::value
  //yeah there's not much to this, just a crosswalk
  template<vk::ClearValue CV, uint64_t ID> struct simpleClear {
    static constexpr clearStep value {
      .id = ID,
      .rr = {
	.id = ID+1
      },
      .clearValue = CV,
    };
  };

#define defineSimpleDepthReference() simpleDepthReference<__LINE__>::value

  template<uint64_t ID> struct simpleDepthReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eFragment,
      .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
      .usage = { vk::DescriptorType::eStorageImage }
    };
  };

#define defineComputeDepthReference() computeDepthReference<__LINE__>::value

  template<uint64_t ID> struct computeDepthReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eCompute,
      .access = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead,
      .usage = { vk::DescriptorType::eStorageImage }
    };
  };

#define defineSimpleColorReference() simpleColorReference<__LINE__>::value

  template<uint64_t ID> struct simpleColorReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eFragment,
      .access = vk::AccessFlagBits2::eColorAttachmentWrite
    };
  };

#define defineComputeColorReference() computeColorReference<__LINE__>::value

  template<uint64_t ID> struct computeColorReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eCompute,
      .access = vk::AccessFlagBits2::eShaderStorageWrite,
      .usage = { vk::DescriptorType::eStorageImage }
    };
  };

#define defineUBReferenceAt(ST) simpleUBReference<__LINE__, ST>::value
#define defineUBReferenceAtVertex() defineUBReferenceAt(vk::ShaderStageFlagBits::eVertex)
#define defineUBReferenceAtCompute() defineUBReferenceAt(vk::ShaderStageFlagBits::eCompute)

  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct simpleUBReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eUniformRead,
      .usage = { vk::DescriptorType::eUniformBuffer }
    };
  };

#define defineSBReadonlyReferenceAt(ST) simpleStorageReadReference<__LINE__, ST>::value

  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct simpleStorageReadReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderStorageRead,
      .frameLatency = 1,
      .usage = { vk::DescriptorType::eStorageBuffer }
    };
  };

#define defineSamplerReference() defineSamplerReferenceAt(vk::ShaderStageFlagBits::eFragment)
#define defineSamplerReferenceAt(ST) samplerReference<__LINE__, ST>::value

  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct samplerReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderSampledRead,
      .frameLatency = 0,
      .usage = { vk::DescriptorType::eCombinedImageSampler, { {}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest } }
    };
  };

  struct cubeFactoryData {
    uint64_t gpuId;
    uint64_t id_seed;
    resourceReference RR_rpColor, RR_rpDepth;
    literalList<uint64_t> RR_shaderTargetGlobalData, RR_shaderTargetPerSideData, RR_shaderSourceCubeSamplers;
    vk::Format format = Format::RGB32float;
    literalList<resourceMap> otherSourceResources;
    literalList<resourceMap> otherTargetResources;
  };

  template<cubeFactoryData CFD, typename TD = glm::vec4> struct cubeFactory {

    static constexpr uint64_t id_seed = CFD.id_seed, sourceIdSeed = id_seed + 100, targetIdSeed = id_seed + 200, targetPerSideIdSeed = id_seed + 300, targetPerSideIdStride = 100, gpuId = CFD.gpuId;

    static constexpr uint64_t sideId(uint32_t side, uint32_t idx) {
      return targetPerSideIdSeed + targetPerSideIdStride * side + idx;
    };

    struct targetViewportData_t {
      glm::mat4 transform;
    };

    struct targetData_t {
      targetViewportData_t perViewport[6];
      TD targetGlobals;
    };

    static constexpr bufferRequirements BR_targetData {
      .deviceId = gpuId,
      .id = id_seed,
      .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .size = sizeof(targetData_t),
      .frameswapCount = 1,
      .hostVisible = false,
    };

    static constexpr bufferRequirements BRS_targetData {
      .deviceId = gpuId,
      .id = id_seed + 1,
      .usage = vk::BufferUsageFlagBits::eTransferSrc,
      .size = sizeof(targetData_t),
      .frameswapCount = 2,
      .hostVisible = true,
    };

    static constexpr imageRequirements IR_cubeColor {
      .deviceId = gpuId,
      .id = id_seed + 10,
      .format = CFD.format,
      .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
      .dimensions = 2,
      .frameswapCount = 2,
      .isCube = true,
      .hostVisible = false,
      .arrayLayers = 6,
    };

    static constexpr imageRequirements IR_depth {
      //not a cube because it doesn't need to be read as a cube, so just hand each side it's own image
      .deviceId = gpuId,
      .id = id_seed + 11,
      .format = Format::D16unorm,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .dimensions = 2,
      .frameswapCount = 1,
      .hostVisible = false,
    };

    static constexpr copyStep C_updateTargetData {
      .id = targetIdSeed,
      .src = {
	.id = targetIdSeed + 1,
	.frameLatency = 1,
      },
      .dst = {
	.id = targetIdSeed + 2,
      },
    };

    static constexpr resourceMap RMS_cube {
      .id = sourceIdSeed + 1,
      .requirementId = IR_cubeColor.id,
      .resourceReferences = CFD.RR_shaderSourceCubeSamplers,
      .isCube = true,
      .resizeBehavior = { { imageResizeType::eBlit } },
    };

    static constexpr resourceMap RMS_targetData_staging {
      .id = sourceIdSeed + 2,
      .requirementId = BRS_targetData.id,
      .resourceReferences = C_updateTargetData.src.id,
    };

    static constexpr resourceMap RMS_targetData {
      .id = sourceIdSeed + 3,
      .requirementId = BR_targetData.id,
      .resourceReferences = C_updateTargetData.dst.id,
    };

    static constexpr resourceMap SL_auto[] = { RMS_targetData, RMS_targetData_staging, RMS_cube };

    static constexpr auto SL_resources = concat<resourceMap, SL_auto, CFD.otherSourceResources>();

    static constexpr sourceLayout SL = {
      .id = sourceIdSeed + 4,
      .resources = SL_resources,
    };

    static_assert((CFD.RR_rpColor.access & vk::AccessFlagBits2::eColorAttachmentWrite) &&
		  (CFD.RR_rpColor.stages & vk::ShaderStageFlagBits::eFragment) &&
		  (CFD.RR_rpDepth.access & vk::AccessFlagBits2::eDepthStencilAttachmentWrite) &&
		  (CFD.RR_rpDepth.stages & vk::ShaderStageFlagBits::eFragment));

    struct targetTemplates_t {
      resourceMap RMT_sideImage, RMT_sideData, RMT_globalData, RMT_sideDepth;
      copyableArray<resourceMap, 4> RMT_auto;
      copyableArray<resourceMap, 4+CFD.otherTargetResources.len> RMT_all;
      targetLayout TL_side;

      constexpr targetTemplates_t(uint32_t i) :
	RMT_sideImage({
	  .id = sideId(i, 0),
	  .requirementId = IR_cubeColor.id,
	  .resourceReferences = CFD.RR_rpColor.id,
	  .external = true,
	  .subresource = { { vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, (uint32_t)i, 1 } },
	}),

	RMT_sideData({
	  .id = sideId(i, 1),
	  .requirementId = BR_targetData.id,
	  .resourceReferences = CFD.RR_shaderTargetPerSideData,
	  .external = true,
	  .subresource = { sizeof(targetViewportData_t)*i, sizeof(targetViewportData_t) },
	  }),

	RMT_globalData({
	  .id = sideId(i, 2),
	  .requirementId = BR_targetData.id,
	  .resourceReferences = CFD.RR_shaderTargetGlobalData,
	  .external = true,
	  .subresource = { sizeof(targetViewportData_t)*6, sizeof(TD) },
	}),

	RMT_sideDepth({
	  .id = sideId(i, 3),
	  .requirementId = IR_depth.id,
	  .resourceReferences = CFD.RR_rpDepth.id,
	  .external = true,
	  .subresource = { { vk::ImageAspectFlagBits::eDepth, 0, VK_REMAINING_MIP_LEVELS, (uint32_t)i, 1 } },
	}),

	RMT_auto({ RMT_sideImage, RMT_sideData, RMT_globalData, RMT_sideDepth }),
	RMT_all([this](size_t i) { return i < 4 ? RMT_auto[i] : CFD.otherTargetResources[i-4]; }),

	TL_side({
	  .id = sideId(i, 3),
	  .resources = RMT_all,
	}) {};

    };

    static constexpr copyableArray<targetTemplates_t, 6> targetTemplates = [](size_t i) {
      return targetTemplates_t(i);
    };

    static constexpr copyableArray<targetLayout, 6> TL_sides = [](size_t i) {
      return targetTemplates[i].TL_side;
    };

  };

};
