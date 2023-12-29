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

#define defineSimpleDepth(gpuId) simpleDepth<gpuId, __LINE__>::value

  template<size_t GPUID, uint64_t ID> struct simpleDepth {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = Format::D16unorm,//drivers are REQUIRED by vulkan to support this format for depth operations
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
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,//transfer src is needed by window to present
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

#define defineSimpleDepthReference() simpleDepthReference<__LINE__>::value

  template<uint64_t ID> struct simpleDepthReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eFragment,
      .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead
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

#define defineSimpleTransformReference() simpleTransformReference<__LINE__>::value

  template<uint64_t ID> struct simpleTransformReference {
    static constexpr resourceReference value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eVertex,
      .access = vk::AccessFlagBits2::eUniformRead,
      .usage = { vk::DescriptorType::eUniformBuffer }
    };
  };

  inline glm::dmat4 makeCameraProjection(float fovDegrees, float width, float height, float near, float far,
					 glm::dvec3 origin, glm::dvec3 focus, glm::dvec3 up) {
    return glm::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1) * //clip
      glm::perspectiveFov<double>(glm::radians(fovDegrees), width, height, near, far) * //projection
      glm::lookAt(origin, focus, up);
  };

  inline glm::dmat4 makeCameraProjection(float fovDegrees, window& w, float near, float far,
					 glm::dvec3 origin, glm::dvec3 focus, glm::dvec3 up) {
    auto size = w.getSize();
    return makeCameraProjection(fovDegrees, size.width, size.height, near, far, origin, focus,  up);
  };

};
