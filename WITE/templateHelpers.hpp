#pragma once

#include "wite_vulkan.hpp"
#include "templateStructs.hpp"
#include "literalList.hpp"
#include "buffer.hpp"

namespace WITE {

  namespace reservedIds {
    uint64_t base = 0xFFFFFFFF00000000 - __LINE__,
      cubeTransform = base + __LINE__,
      cubeTransformStaging = base + __LINE__,
  };

  template<class T> consteval T withId(T t, uint64_t id) {
    T ret = t;
    ret.id = id;
    return ret;
  };

#define NEW_ID(t) WITE::withId(t, __LINE__)

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

    static constexpr resourceConsumer resourceConsumer_v = {
      .id = ID+1,
      .stages = vk::ShaderStageFlagBits::eVertex,
      .access = vk::AccessFlagBits2::eVertexAttributeRead,
      .usage = { U, instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex },
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

  constexpr resizeBehavior_t resize_trackWindow_discard { imageResizeType::eDiscard, {}, true };
  constexpr resizeBehavior_t resize_none { imageResizeType::eNone, {}, false };

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
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,//transfer src is needed by window to present, transfer dst to clear (unless on a RP)
      .frameswapCount = 2
    };
  };

#define defineIntermediateColor(gpuId) intermediateColor<gpuId, __LINE__>::value
#define defineIntermediateColorWithFormat(gpuId, F) intermediateColor<gpuId, __LINE__, F>::value
  template<size_t GPUID, uint64_t ID, vk::Format F = RGBA8Unorm> struct intermediateColor {
    static constexpr imageRequirements value {
      .deviceId = gpuId,
      .id = ID,
      .format = F,
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
      .frameswapCount = 1
    };
  };

#define defineCopy() simpleCopy<__LINE__ * 1000000>::value
  //yeah there's not much to this, just a crosswalk
  template<uint64_t ID> struct simpleCopy {
    static constexpr copyStep value {
      .id = ID,
      .src = ID+1,
      .dst = ID+2,
    };
  };

#define defineClear(...) simpleClear<vk::ClearValue {{ __VA_ARGS__ }}, __LINE__>::value
  //not much to this, either
  template<vk::ClearValue CV, uint64_t ID> struct simpleClear {
    static constexpr clearStep value {
      .id = ID,
      .clearValue = CV,
    };
  };

#define defineUBConsumer(ST) simpleUBConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct simpleUBConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eUniformRead,
      .usage = { vk::DescriptorType::eUniformBuffer }
    };
  };

#define defineSBReadonlyConsumer(ST) simpleStorageReadConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct simpleStorageReadConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderStorageRead,
      .usage = { vk::DescriptorType::eStorageBuffer }
    };
  };

#define defineSamplerConsumer(ST) samplerConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlagBits ST> struct samplerConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderSampledRead,
      .usage = { vk::DescriptorType::eCombinedImageSampler, { {}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest } }
    };
  };

  //often, a specialization is quite simple, like a single constant or struct.
#define defineShaderSpecialization(T, D, N) typedef shaderSpecializationWrapper_t<T, D> N;
#define forwardShaderSpecialization(N) N ::map, (void*)& N ::value, sizeof(N ::value_t)
  template<class T, const T D> struct shaderSpecializationWrapper_t {
    typedef const T value_t;
    static constexpr value_t value = D;
    static constexpr vk::SpecializationMapEntry map { 0, 0, sizeof(T) };
  };

  struct cubeTransform_t {
    glm::mat4 transforms[6];
  };

  constexpr bufferRequirements BR_cubeTransform = withId(defineSimpleUniformBuffer(gpuId, sizeof(cubeTransform_t)), reservedIds::cubeTransform);
  constexpr bufferRequirements BR_S_cubeTransform = withId(stagingRequirementsFor(BR_cubeTransform, 2), reservedIds::cubeTransformStaging);

  template<literalList<resourceReference> allSideReferences,
	   literalList<uint64_t> cameraTransformConsumers,
	   uint64_t transformSlotId,
	   uint64_t idBase,
	   uint64_t objectLayoutId>
  struct cubeHelper {
    //TODO more stuff inside, fewer template arguments
    //depth gets its own ll of consumers, so can be empty
    static constexpr size_t rrCount = allSideReferences.len + cameraTransformConsumers.len;//per side
    static constexpr uint64_t targetIdBase = idBase + 100;

    static constexpr copyableArray<copyableArray<resourceReference, rrCount>, 6> targetRRS = [](size_t i) {
      return [i](size_t j) {
	return j < 6 ? resourceReference(cameraTransformConsumers[j],
					 transformSlotId,
					 { i * sizeof(glm::mat4), sizeof(glm::mat4) })
	  : allSideReferences[j-6];
      };
    };

    static constexpr copyableArray<targetLayout, 6> TLS = [](size_t i) {
      return targetLayout(targetIdBase + i, objectLayoutId, targetRRS[i]);
    };

  };

};
