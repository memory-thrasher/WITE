/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "wite_vulkan.hpp"
#include "onionTemplateStructs.hpp"
#include "literalList.hpp"
#include "buffer.hpp"

namespace WITE {

  template<class T> consteval T withId(T t, uint64_t id) {
    T ret = t;
    ret.id = id;
    return ret;
  };

#define NEW_ID(t) ::WITE::withId(t, __LINE__)

#define wrap_mesh(GPU, U, NOM, ...) constexpr ::WITE::meshWrapper< GPU, U, ::WITE::countIL<udmObject<U>>({ __VA_ARGS__ }), __LINE__ * 1000000 > NOM = { __VA_ARGS__ }
#define wrap_mesh_fromArray(GPU, U, NOM, M) constexpr ::WITE::meshWrapper< GPU, U, sizeof(M)/::WITE::sizeofUdm<U>(), __LINE__ * 1000000 > NOM = M

  //produces configuration structs with id values of [ID, ID+2]
  template<size_t gpuId, udm U, size_t C, uint64_t ID, bool instance = false> struct meshWrapper {

    static_assert(C > 0);

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
    constexpr meshWrapper(const udmObject<U> m[C]) : mesh(m) {};

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

#define defineSimpleUniformBuffer(gpuId, size) ::WITE::simpleUB<gpuId, __LINE__, size>::value
  template<size_t GPUID, uint64_t ID, uint32_t size> struct simpleUB {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
      .size = size,
      .frameswapCount = 1,//staging should be frameswapped, updated data will blow away this buffer every frame
    };
  };

#define defineSwappedUniformBuffer(gpuId, size) ::WITE::swappableUB<gpuId, __LINE__, size>::value
  template<size_t GPUID, uint64_t ID, uint32_t size> struct swappableUB {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
      .size = size,
      .frameswapCount = 2,
    };
  };

#define defineSimpleStorageBuffer(gpuId, size) ::WITE::simpleSB<gpuId, __LINE__, size>::value
  template<size_t GPUID, uint64_t ID, uint32_t size> struct simpleSB {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
      .size = size,
      .frameswapCount = 1,
    };
  };

#define defineSingleTransform(gpuId) ::WITE::singleTransform<gpuId, __LINE__>::value
  template<size_t GPUID, uint64_t ID> struct singleTransform {
    static constexpr bufferRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
      .size = sizeof(glm::mat4),
      .frameswapCount = 1,//staging should be frameswapped, updated transform will blow away this buffer every frame
    };
  };

#define defineComputeDepth(gpuId) ::WITE::computeDepth<gpuId, __LINE__>::value
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

#define defineSimpleDepth(gpuId) ::WITE::simpleDepth<gpuId, __LINE__>::value
  template<size_t GPUID, uint64_t ID> struct simpleDepth {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = Format::D16unorm,//drivers are REQUIRED by vulkan to support this format for depth operations (and blit src)
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .frameswapCount = 1
    };
  };

#define defineSimpleColor(gpuId) ::WITE::simpleColor<gpuId, __LINE__>::value
  template<size_t GPUID, uint64_t ID> struct simpleColor {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = Format::RGBA8unorm,//drivers are REQUIRED by vulkan to support this format for most operations (including color attachment)
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,//transfer src is needed by window to present, transfer dst to clear (unless on a RP)
      .frameswapCount = 2
    };
  };

#define defineIntermediateColor(gpuId) ::WITE::intermediateColor<gpuId, __LINE__>::value
#define defineIntermediateColorWithFormat(gpuId, F) ::WITE::intermediateColor<gpuId, __LINE__, F>::value
  template<size_t GPUID, uint64_t ID, vk::Format F = Format::RGBA8unorm> struct intermediateColor {
    static constexpr imageRequirements value {
      .deviceId = GPUID,
      .id = ID,
      .format = F,
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
      .frameswapCount = 1
    };
  };

#define defineCopy() ::WITE::simpleCopy<__LINE__ * 1000000>::value
  //yeah there's not much to this, just a crosswalk
  template<uint64_t ID> struct simpleCopy {
    static constexpr copyStep value {
      .id = ID,
      .src = ID+1,
      .dst = ID+2,
    };
  };

#define defineClear(...) ::WITE::simpleClear<vk::ClearValue {{ __VA_ARGS__ }}, __LINE__>::value
  //not much to this, either
  template<vk::ClearValue CV, uint64_t ID> struct simpleClear {
    static constexpr clearStep value {
      .id = ID,
      .clearValue = CV,
    };
  };

#define defineUBConsumer(ST) ::WITE::simpleUBConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlags ST> struct simpleUBConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eUniformRead,
      .usage = { vk::DescriptorType::eUniformBuffer }
    };
  };

#define defineSBReadonlyConsumer(ST) ::WITE::simpleStorageReadConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlags ST> struct simpleStorageReadConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderStorageRead,
      .usage = { vk::DescriptorType::eStorageBuffer }
    };
  };

#define defineSBWriteonlyConsumer(ST) ::WITE::simpleStorageWriteConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlags ST> struct simpleStorageWriteConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderStorageWrite,
      .usage = { vk::DescriptorType::eStorageBuffer }
    };
  };

#define defineSamplerConsumer(ST) ::WITE::samplerConsumer<__LINE__, vk::ShaderStageFlagBits::e ##ST>::value
  template<uint64_t ID, vk::ShaderStageFlags ST> struct samplerConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = ST,
      .access = vk::AccessFlagBits2::eShaderSampledRead,
      .usage = { vk::DescriptorType::eCombinedImageSampler, { {}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest } }
    };
  };

#define defineIndirectConsumer() ::WITE::indirectConsumer<__LINE__>::value
  template<uint64_t ID> struct indirectConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eVertex,
      .access = vk::AccessFlagBits2::eIndirectCommandRead,
      .usage = resourceUsageType::eIndirect,
    };
  };

#define defineIndirectCountConsumer() ::WITE::indirectCountConsumer<__LINE__>::value
  template<uint64_t ID> struct indirectCountConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eVertex,
      .access = vk::AccessFlagBits2::eIndirectCommandRead,
      .usage = resourceUsageType::eIndirectCount,
    };
  };

#define defineIndirectMeshConsumer() ::WITE::indirectMeshConsumer<__LINE__>::value
  template<uint64_t ID> struct indirectMeshConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eTaskEXT,
      .access = vk::AccessFlagBits2::eIndirectCommandRead,
      .usage = resourceUsageType::eIndirect,
    };
  };

#define defineIndirectMeshCountConsumer() ::WITE::indirectMeshCountConsumer<__LINE__>::value
  template<uint64_t ID> struct indirectMeshCountConsumer {
    static constexpr resourceConsumer value {
      .id = ID,
      .stages = vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eTaskEXT,
      .access = vk::AccessFlagBits2::eIndirectCommandRead,
      .usage = resourceUsageType::eIndirectCount,
    };
  };

  //often, a specialization is quite simple, like a single constant or struct.
#define defineShaderSpecialization(T, D, N) typedef ::WITE::shaderSpecializationWrapper_t<T, D> N;
#define forwardShaderSpecialization(N) N ::map, (void*)& N ::value, sizeof(N ::value_t)
  template<class T, const T D> struct shaderSpecializationWrapper_t {
    typedef const T value_t;
    static constexpr value_t value = D;
    static constexpr vk::SpecializationMapEntry map { 0, 0, sizeof(T) };
  };

  struct cubeTransform_t {
    glm::mat4 transforms[6];
  };

  struct cubeTargetData {
    uint64_t gpuId, idBase;
    objectLayout OL;
    literalList<uint64_t> cameraTransformConsumers;
    literalList<uint64_t> cameraColorConsumers;
    literalList<uint64_t> cameraDepthConsumers;
    literalList<resourceReference> otherTargetReferences;
  };

  template<cubeTargetData CD> struct cubeTargetHelper {
    static constexpr uint64_t targetIdBase = CD.idBase + 100,
      objectLayoutId = CD.OL.id,
      requirementsBaseId = CD.idBase + 200,
      slotIdBase = CD.idBase + 300;

    static constexpr bool hasDepth = CD.cameraDepthConsumers.len;

    static constexpr bufferRequirements BR_cubeTransform = withId(defineSimpleUniformBuffer(CD.gpuId, sizeof(cubeTransform_t)), requirementsBaseId);
    static constexpr bufferRequirements BR_S_cubeTransform = withId(stagingRequirementsFor(BR_cubeTransform, 2), requirementsBaseId + 1);

    static constexpr imageRequirements IR_color = {
      .deviceId = CD.gpuId,
      .id = requirementsBaseId + 2,
      .format = Format::RGBA32float,
      .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
      .dimensions = 2,
      .frameswapCount = 2,
      .isCube = true,
      .arrayLayers = 6,
      .mipLevels = 1,//for now
    }, IR_depth = {
      .deviceId = CD.gpuId,
      .id = requirementsBaseId + 3,
      .format = Format::D16unorm,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .dimensions = 2,
      .frameswapCount = 1,
      .isCube = true,
      .arrayLayers = 6,
    };

    static constexpr resourceSlot RS_cubemapCamera_trans_staging = {
      .id = slotIdBase + 0,
      .requirementId = BR_S_cubeTransform.id,
      .objectLayoutId = objectLayoutId,
    }, RS_cubemapCamera_trans = {
      .id = slotIdBase + 1,
      .requirementId = BR_cubeTransform.id,
      .objectLayoutId = objectLayoutId,
    }, RS_cubemapCamera_color = {
      .id = slotIdBase + 2,
      .requirementId = IR_color.id,
      .objectLayoutId = objectLayoutId,
      .resizeBehavior = resize_none,
    }, RS_cubemapCamera_depth = {
      .id = slotIdBase + 3,
      .requirementId = IR_depth.id,
      .objectLayoutId = objectLayoutId,
      .resizeBehavior = resize_none,
    };

    static constexpr resourceSlot RS_all[] = {
      RS_cubemapCamera_trans_staging,
      RS_cubemapCamera_trans,
      RS_cubemapCamera_color,
      RS_cubemapCamera_depth,
    };

    static constexpr copyableArray<resourceSlot, sizeof(RS_all) / sizeof(RS_all[0]) - int(hasDepth ? 0 : 1)> RS_used = RS_all;

    static constexpr size_t targetRRS_c = CD.cameraDepthConsumers.len + CD.cameraColorConsumers.len + CD.cameraTransformConsumers.len + CD.otherTargetReferences.len;

    template<uint8_t sideId> struct sideData {
      static constexpr copyableArray<resourceReference, CD.cameraTransformConsumers.len> targetTransformRRS = [](size_t j) {
	return resourceReference { CD.cameraTransformConsumers[j], RS_cubemapCamera_trans.id, 0,
				   { sideId * sizeof(glm::mat4), sizeof(glm::mat4) }};
      };
      static constexpr copyableArray<resourceReference, CD.cameraColorConsumers.len> targetColorRRS = [](size_t j) {
	return resourceReference { CD.cameraColorConsumers[j], RS_cubemapCamera_color.id, 1,
				   {{ vk::ImageAspectFlagBits::eColor, 0, IR_color.mipLevels, sideId, 1 }}};
      };
      static constexpr copyableArray<resourceReference, CD.cameraDepthConsumers.len> targetDepthRRS = [](size_t j) {
	return resourceReference { CD.cameraDepthConsumers[j], RS_cubemapCamera_depth.id, 0,
				   {{ vk::ImageAspectFlagBits::eDepth, 0, IR_depth.mipLevels, sideId, 1 }}};
      };
      static constexpr copyableArray<resourceReference, targetRRS_c> allRRS =
	concat<resourceReference, targetTransformRRS, targetColorRRS, targetDepthRRS, CD.otherTargetReferences>();
    };

    static constexpr copyableArray<targetLayout, 6> TLS = {
      { targetIdBase + 0, objectLayoutId, sideData<0>::allRRS },
      { targetIdBase + 1, objectLayoutId, sideData<1>::allRRS },
      { targetIdBase + 2, objectLayoutId, sideData<2>::allRRS },
      { targetIdBase + 3, objectLayoutId, sideData<3>::allRRS },
      { targetIdBase + 4, objectLayoutId, sideData<4>::allRRS },
      { targetIdBase + 5, objectLayoutId, sideData<5>::allRRS },
    };

    template<onionDescriptor OD>
    static void updateTargetLocation(glm::vec3 l, typename onion<OD>::template object_t<objectLayoutId>* o) {
      //see vulkan standard, "Cube Map Face Selection". World-to-camera. Shaders handle perspective division.
      //the cube map is always oriented along world axes, regardless of the orientation of the object.
      cubeTransform_t ct {{//note: colum major
	  { 0, 0, 1, 0,   0,-1, 0, 0,  -1, 0, 0, 0,    l.z,  l.y, -l.x, 1 },// +x
	  { 0, 0,-1, 0,   0,-1, 0, 0,   1, 0, 0, 0,   -l.z,  l.y,  l.x, 1 },// -x
	  { 1, 0, 0, 0,   0, 0, 1, 0,   0, 1, 0, 0,   -l.x, -l.z, -l.y, 1 },// +y
	  { 1, 0, 0, 0,   0, 0,-1, 0,   0,-1, 0, 0,   -l.x,  l.z,  l.y, 1 },// -y
	  { 1, 0, 0, 0,   0,-1, 0, 0,   0, 0, 1, 0,   -l.x,  l.y, -l.z, 1 },// +z
	  {-1, 0, 0, 0,   0,-1, 0, 0,   0, 0,-1, 0,    l.x,  l.y,  l.z, 1 },// -z
	}};
      o->template write<RS_cubemapCamera_trans_staging.id>(ct);
    };

  };

#define cubeHelper_unpackIRS(CTH) CTH ::IR_color, CTH ::IR_depth
#define cubeHelper_unpackBRS(CTH) CTH ::BR_cubeTransform, CTH ::BR_S_cubeTransform

};
