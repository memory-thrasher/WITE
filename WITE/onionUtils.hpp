#pragma once

#include <vector>

#include "templateStructs.hpp"
#include "math.hpp"
#include "DEBUG.hpp"

namespace vk {
  template <> struct FlagTraits<WITE::imageFlags_e> {
    static constexpr bool             isBitmask = true;
    static constexpr WITE::imageFlags_t allFlags = WITE::imageFlags_e::eCube | WITE::imageFlags_e::e3DIs2DArray |
      WITE::imageFlags_e::eHostVisible;
  };
}

namespace WITE {

  consteval void appendFlow(imageRequirements& r, imageRequirements::flowStep f) {
    if(r.imageFlags & imageFlags_e::eAlwaysGeneralLayout)
      return;//already in always general layout mode, nothing to do
    if(r.flowCount > 0 && r.flow[r.flowCount-1].layout == f.layout) {
      size_t lastIdx = r.flowCount-1;
      //two flows in a row with the same layout can be merged
      r.flow[lastIdx].stages |= f.stages;
      r.flow[lastIdx].access |= f.access;
      if(r.flow[lastIdx].onionId != f.onionId) r.flow[lastIdx].onionId = NONE;
      if(r.flow[lastIdx].shaderId != f.shaderId) r.flow[lastIdx].shaderId = NONE;
      return;
    }
    if(r.flowCount == 0) {
      //append to empty list, trivial
      r.flowCount = 1;
      r.flow[0] = f;
      return;
    }
    if(f.layout == vk::ImageLayout::eGeneral || r.flowCount == imageRequirements::MAX_FLOW_STEPS) {
      //new layout is general, or overflow, convert the entire image requirement to always general mode
      r.imageFlags |= imageFlags_e::eAlwaysGeneralLayout;
      r.flowCount = 0;
      return;
    }
    //and finally, ordinary append
    r.flow[r.flowCount++] = f;
  };

  //note: this does not create a flow
  consteval imageRequirements requirementsForSlot(const shaderImageSlot& sis) {
    imageRequirements ret {
      .format = sis.format,
      .dimensions = sis.dimensions,
      .frameswapCount = sis.frameswapCount,
      .imageFlags = sis.imageFlags,
      .arrayLayers = sis.arrayLayers,
      .mipLevels = sis.mipLevels
    };
    if(sis.writeStages) ret.usage |= vk::ImageUsageFlagBits::eStorage;
    if(sis.readStages) ret.usage |= vk::ImageUsageFlagBits::eSampled;
    return ret;
  };

  //note: this does not create a flow
  consteval imageRequirements requirementsForSlot(const shaderAttachmentSlot& sis) {
    return {
      .format = sis.format
    };
  };

  constexpr bool isValid(const imageRequirements& ir) {
    return (ir.dimensions > 1)
      && (ir.dimensions < 4)
      && (ir.arrayLayers > 0)
      && (ir.dimensions < 3 || ir.arrayLayers == 1)
      && (ir.frameswapCount > 0)
      && (ir.mipLevels > 0)
      && (ir.mipLevels == 1 || ir.dimensions > 1)
      && ((~ir.imageFlags & imageFlags_e::eCube) || (ir.arrayLayers % 6 == 0))
      && ((~ir.imageFlags & imageFlags_e::e3DIs2DArray) || (ir.dimensions == 3))
      //flow needed if and only if we're not using general mode
      && (bool(ir.imageFlags & imageFlags_e::eAlwaysGeneralLayout) ^ (ir.flowCount > 0));
  };

  inline void assertValidRuntime(const imageRequirements& ir) {
    ASSERT_TRAP(isValid(ir), "invalid image requirements (runtime check)");
  };

  consteval void assertValidCE(const imageRequirements& ir) {
    ASSERT_CONSTEXPR(isValid(ir));
  };

  consteval imageRequirements& operator&=(imageRequirements& l, const imageRequirements& r) {
    if(l.format == vk::Format::eUndefined) l.format = r.format;
    ASSERT_CONSTEXPR(l.format == r.format || r.format == vk::Format::eUndefined);
    if(l.frameswapCount == 0) l.frameswapCount = r.frameswapCount;
    ASSERT_CONSTEXPR(l.frameswapCount == r.frameswapCount || r.frameswapCount == 0);
    if(l.mipLevels == 0) l.mipLevels = r.mipLevels;
    ASSERT_CONSTEXPR(l.mipLevels == r.mipLevels || r.mipLevels == 0);
    if(l.deviceId == NONE) l.deviceId = r.deviceId;
    ASSERT_CONSTEXPR(l.deviceId == r.deviceId || r.deviceId == NONE);
    if(r.imageFlags & imageFlags_e::eAlwaysGeneralLayout) {
      l.imageFlags |= imageFlags_e::eAlwaysGeneralLayout;
      l.flowCount = 0;
    }
    if(~l.imageFlags & imageFlags_e::eAlwaysGeneralLayout)
      for(size_t i = 0;i < r.flowCount;i++)
	appendFlow(l, r.flow[i]);
    if(min(l.dimensions, r.dimensions) == 2 && max(l.dimensions, r.dimensions) == 3) {
      //special case: a 3D image can be used as an array of 2D images. In no case may a 3D image have >1 array layer.
      l.dimensions = 3;
      l.arrayLayers = 1;
    } else {
      if(l.dimensions == 0)
	l.dimensions = r.dimensions;
      ASSERT_CONSTEXPR(l.dimensions == r.dimensions || r.dimensions == 0);
      l.arrayLayers = max(l.arrayLayers, r.arrayLayers);
    }
    l.usage |= r.usage;
    l.imageFlags |= r.imageFlags;
    return l;
  };

  //imageRequirements is NOT a bitmask. & means union, as in "find criteria that will satisfy both these sets of criteria"
  consteval imageRequirements operator&(const imageRequirements& l, const imageRequirements& r) {
    imageRequirements ret { l };
    ret &= r;
    return ret;
  };

  consteval imageRequirements& operator&=(imageRequirements& l, const shaderAttachmentSlot& r) {
    return operator&=(l, requirementsForSlot(r));
  };

  consteval imageRequirements operator&(const imageRequirements& l, const shaderAttachmentSlot& r) {
    return operator&(l, requirementsForSlot(r));
  };

  consteval imageRequirements& operator&=(imageRequirements& l, const shaderImageSlot& r) {
    return operator&=(l, requirementsForSlot(r));
  };

  consteval imageRequirements operator&(const imageRequirements& l, const shaderImageSlot& r) {
    return operator&(l, requirementsForSlot(r));
  };

  consteval shaderAttachmentSlot attach(shaderImageSlot sis) {
    return {
      .id = sis.id,
      .format = sis.format
    };
  }

  template<uint64_t id, uint64_t onionId, const literalList<shader> shaders, uint64_t gpuId>
  consteval imageRequirements getImageRequirements() {
    imageRequirements ret {
      .deviceId = gpuId
    };
    for(size_t i = 0;i < shaders.len;i++) {
      const shader& s = shaders[i];
      imageRequirements::flowStep flow {
	.onionId = onionId,
	.shaderId = s.id,
	.layout = vk::ImageLayout::eUndefined
      };
      for(const shaderImageSlot& sis : s.sampled)
	if(sis.id == id) {
	  ret &= requirementsForSlot(sis);
	  if(flow.layout != vk::ImageLayout::eGeneral)
	    flow.layout = sis.writeStages ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
	  flow.stages |= sis.readStages | sis.writeStages;
	  if(sis.writeStages) {
	    flow.access |= vk::AccessFlagBits2::eShaderStorageWrite;
	    if(sis.readStages)
	      flow.access |= vk::AccessFlagBits2::eShaderStorageRead;
	  } else
	    flow.access |= vk::AccessFlagBits2::eShaderSampledRead;
	}
      if(s.targetLink.depthStencil.id == id) {
	ASSERT_CONSTEXPR(flow.layout == vk::ImageLayout::eUndefined);
	flow.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	flow.stages |= vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
	flow.access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
	ret.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	ret &= requirementsForSlot(s.targetLink.depthStencil);
      }
      if(s.targetLink.color.id == id) {
	ASSERT_CONSTEXPR(flow.layout == vk::ImageLayout::eUndefined);
	flow.layout = vk::ImageLayout::eColorAttachmentOptimal;
	flow.stages |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	flow.access |= vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
	ret.usage |= vk::ImageUsageFlagBits::eColorAttachment;
	ret &= requirementsForSlot(s.targetLink.color);
      }
      if(flow.layout != vk::ImageLayout::eUndefined)
	appendFlow(ret, flow);
    }
    return ret;
  };

  consteval auto getDefaultCI(imageRequirements r) {
    assertValidCE(r);
    vk::ImageCreateInfo ret;
    ret.setImageType((vk::ImageType)(r.dimensions-1))//yes, I'm that lazy
      .setFormat(r.format)
      .setMipLevels(r.mipLevels)
      .setArrayLayers(r.arrayLayers)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling((r.imageFlags & imageFlags_e::eHostVisible) ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal)
      .setUsage(r.usage)
      .setSharingMode(vk::SharingMode::eExclusive);
    if(r.imageFlags & imageFlags_e::eCube) ret.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    if(r.imageFlags & imageFlags_e::e3DIs2DArray) ret.flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
    return ret;
  };

  consteval bufferRequirements requirementsForSlot(const shaderUniformBufferSlot& subs) {
    return {
      .usage = subs.writeStages ? vk::BufferUsageFlagBits::eStorageBuffer : vk::BufferUsageFlagBits::eUniformBuffer,
      .readStages = subs.readStages,
      .writeStages = subs.writeStages,
      .size = subs.size,
      .frameswapCount = subs.frameswapCount
    };
  };

  template<shaderVertexBufferSlot svbs>//sizeofUdm must be served the format as a template argument
  consteval bufferRequirements requirementsForSlot() {
    return {
      .usage = vk::BufferUsageFlagBits::eVertexBuffer,
      .readStages = vk::PipelineStageFlagBits2::eVertexInput,
      .size = svbs.vertCount * sizeofUdm<svbs.format>(),
      .frameswapCount = svbs.frameswapCount
    };
  };

  consteval bufferRequirements& operator&=(bufferRequirements& l, bufferRequirements r) {
    l.usage |= r.usage;
    l.readStages |= r.readStages;
    l.writeStages |= r.writeStages;
    l.size = max(l.size, r.size);
    if(l.frameswapCount == 0) l.frameswapCount = r.frameswapCount;
    ASSERT_CONSTEXPR(l.frameswapCount == r.frameswapCount || r.frameswapCount == 0);
    if(l.deviceId == NONE) l.deviceId = r.deviceId;
    ASSERT_CONSTEXPR(l.deviceId == r.deviceId || r.deviceId == NONE);
    return l;
  };

  //bufferRequirements is NOT a bitmask. & means union, as in "find criteria that will satisfy both these sets of criteria"
  consteval bufferRequirements operator&(const bufferRequirements& l, const bufferRequirements& r) {
    bufferRequirements ret { l };
    ret &= r;
    return ret;
  };

  consteval bufferRequirements& operator&=(bufferRequirements& l, const shaderUniformBufferSlot& r) {
    return operator&=(l, requirementsForSlot(r));
  };

  consteval bufferRequirements operator&(const bufferRequirements& l, const shaderUniformBufferSlot& r) {
    return operator&(l, requirementsForSlot(r));
  };

  /// L <= R means "is L a subset of R", which means "does any resource that satisfies R always satisfy L"
  consteval bool operator<=(const bufferRequirements& l, const bufferRequirements& r) {
    return l.deviceId == r.deviceId && !(l.memoryFlags & ~r.memoryFlags) && !(l.usage & ~r.usage) && !(l.readStages & ~r.readStages) && !(l.writeStages & ~r.writeStages) && l.size <= r.size && l.frameswapCount == r.frameswapCount;
  };

  /// L <= R means "is L a subset of R", which means "does any resource that satisfies R always satisfy L"
  consteval bool operator<=(const imageRequirements& l, const imageRequirements& r) {
    if(l.flowCount && (~l.imageFlags & imageFlags_e::eAlwaysGeneralLayout)) {
      if(r.flowCount == 1 && r.flow[0].layout == vk::ImageLayout::eGeneral) {
	//if general layout is ever used, it's all that's used. So it's a superset of everything
	//onionId and shaderId no longer mean anything but stages and access do
	for(size_t i = 0;i < l.flowCount;i++)
	  if((l.flow[i].stages & ~r.flow[0].stages) || (l.flow[i].access & ~r.flow[0].access))
	    return false;
      } else {
	if(l.flowCount > r.flowCount) return false;
	bool foundMatch = false;
	for(size_t i = 0;i <= r.flowCount - l.flowCount && !foundMatch;i++) {
	  foundMatch = true;
	  for(size_t j = 0;j < l.flowCount && foundMatch;j++) {
	    auto rf = r.flow[i + j],
	      lf = l.flow[j];
	    foundMatch = rf.onionId == lf.onionId && rf.shaderId == lf.shaderId && rf.layout == lf.layout &&
	      !(lf.access & ~rf.access) && !(lf.stages & ~rf.stages);
	  }
	}
	if(!foundMatch) return false;
      }
    }
    //most of these fields, treat 0 as "do not care" for l only
    return (l.deviceId == r.deviceId || l.deviceId == NONE) &&
      (l.format == r.format || l.format == vk::Format::eUndefined) &&
      (l.dimensions == r.dimensions || l.dimensions == 0) &&
      (l.frameswapCount == r.frameswapCount || l.frameswapCount == 0) &&
      (l.arrayLayers <= r.arrayLayers || l.arrayLayers == 0) &&
      (l.mipLevels == r.mipLevels || l.mipLevels == 0) &&
      !((l.imageFlags ^ r.imageFlags) & imageFlags_e::metaMustMatch) &&
      !(l.usage & ~r.usage);
  };

  template<uint64_t id, literalList<shader> shaders>
  consteval bufferRequirements getVertexBufferRequirements() {
    if constexpr(shaders.len == 0)
      return {};
    else {
      bufferRequirements ret;
      constexpr const shader& s = shaders[0];
      if constexpr(s.vertexBuffer && s.vertexBuffer->id == id)
	ret &= requirementsForSlot<*s.vertexBuffer>();
      if constexpr(s.instanceBuffer && s.instanceBuffer->id == id)
	ret &= requirementsForSlot<*s.instanceBuffer>();
      return ret & getVertexBufferRequirements<id, shaders.sub(1)>();
    }
  };

  template<uint64_t id, literalList<shaderVertexBufferSlot> slots>
  consteval bufferRequirements getVertexBufferRequirements() {
    if constexpr(slots.len == 0)
      return {};
    else {
      if constexpr(slots[0].id != id) return getVertexBufferRequirements<id, slots.sub(1)>();
      return requirementsForSlot<slots[0]>() &
	getVertexBufferRequirements<id, slots.sub(1)>();
    }
  };

  template<uint64_t id, shaderTargetLayout TL, literalList<shader> shaders, uint64_t gpuId>
  consteval bufferRequirements getBufferRequirements() {
    bufferRequirements ret {
      .deviceId = gpuId
    };
    ret &= getVertexBufferRequirements<id, shaders>();
    for(const shader& s : shaders)
      for(const shaderUniformBufferSlot& ub : s.uniformBuffers)
	if(ub.id == id)
	  ret &= ub;
    ret &= getVertexBufferRequirements<id, TL.vertexBuffers>();
    ret &= getVertexBufferRequirements<id, TL.instanceBuffers>();
    for(const shaderUniformBufferSlot& ub : TL.uniformBuffers)
      if(ub.id == id)
	ret &= ub;
    return ret;
  };

  constexpr bool isValid(const bufferRequirements& b) {
    return b.usage && (b.readStages | b.writeStages) && b.size && b.frameswapCount && b.deviceId != NONE;
  };

  consteval vk::BufferCreateInfo getDefaultCI(bufferRequirements r) {
    ASSERT_CONSTEXPR(isValid(r));
    vk::BufferCreateInfo ret;
    ret.setSize(r.size);
    ret.setUsage(r.usage);
    return ret;
  };

  template<class T> consteval size_t findId(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return i;
    constexprAssertFailed();
    return l.len;
  };

}
