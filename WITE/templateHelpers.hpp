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
    static constexpr uint64_t objectLayoutID = ID + 1000;//use if resourceSlot_v and resourceReference_v are used

    static constexpr bufferRequirements bufferRequirements_v {
      .deviceId = gpuId,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
      .size = sizeofUdm<U>() * C,
      .frameswapCount = 1,
    };

    static constexpr resourceConsumer resourceConsumer_v = {
      .id = ID+1,
      .stages = vk::ShaderStageFlagBits::eVertex,
      .access = vk::AccessFlagBits2::eVertexAttributeRead,
      .usage = { U, instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex },
    };

    static constexpr resourceSlot resourceSlot_v = {
      .id = ID+2,
      .requirementId = bufferRequirements_v.id,
      .objectLayoutId = objectLayoutID,
      .external = true,
    };

    static constexpr resourceReference resourceReference_v = {
      .id = ID+3,
      .resourceConsumerId = resourceConsumer_v.id,
      .resourceSlotId = resourceSlot_v.id,
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
      .size = sizeof(glm::mat4),
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
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eFragment,
      .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
      .usage = { vk::DescriptorType::eStorageImage }
    };
  };

#define defineComputeDepthReference() computeDepthReference<__LINE__>::value

  template<uint64_t ID> struct computeDepthReference {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eCompute,
      .access = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead,
      .usage = { vk::DescriptorType::eStorageImage }
    };
  };

#define defineSimpleColorReference() simpleColorReference<__LINE__>::value

  template<uint64_t ID> struct simpleColorReference {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eFragment,
      .access = vk::AccessFlagBits2::eColorAttachmentWrite
    };
  };

#define defineComputeColorReference() computeColorReference<__LINE__>::value

  template<uint64_t ID> struct computeColorReference {
    static constexpr resourceConsumer value {
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
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eUniformRead,
      .usage = { vk::DescriptorType::eUniformBuffer }
    };
  };

#define defineSBReadonlyReferenceAt(ST) simpleStorageReadReference<__LINE__, ST>::value

  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct simpleStorageReadReference {
    static constexpr resourceConsumer value {
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
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderSampledRead,
      .frameLatency = 0,
      .usage = { vk::DescriptorType::eCombinedImageSampler, { {}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest } }
    };
  };

};
