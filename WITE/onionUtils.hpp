#pragma once

#include <vector>

#include "templateStructs.hpp"
#include "math.hpp"

namespace vk {
  template <> struct FlagTraits<WITE::imageFlags_e> {
    static constexpr bool             isBitmask = true;
    static constexpr WITE::imageFlags_t allFlags = WITE::imageFlags_e::eCube | WITE::imageFlags_e::e3DIs2DArray |
      WITE::imageFlags_e::eHostVisible;
  };
}

namespace WITE {

  constexpr uint64_t NONE = ~0;

  template<size_t STEP_COUNT> struct imageFlow {

    typedef copyableArray<imageRequirements::flowStep, STEP_COUNT> flow_t;

    flow_t flow;

    template<class T> consteval imageFlow(T t) : flow(t) {};
    consteval imageFlow(imageRequirements::flowStep s) : flow(s) {};
    consteval imageFlow(const imageFlow<STEP_COUNT>& o) = default;

    consteval operator flow_t() const noexcept {
      return flow;
    };

    consteval operator literalList<imageRequirements::flowStep>() const {
      return flow;
    };

    template<size_t OST> consteval imageFlow<STEP_COUNT+OST> then(imageFlow<OST> o) const {
      return flow + o.flow;
    };

    template<size_t OST> consteval imageFlow<STEP_COUNT+OST> operator+(imageFlow<OST> o) const {
      return then(o);
    };

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

  consteval void assertValid(const imageRequirements& ir) {
    ASSERT_CONSTEXPR(ir.flow);
    ASSERT_CONSTEXPR(ir.dimensions > 1);
    ASSERT_CONSTEXPR(ir.dimensions < 4);
    ASSERT_CONSTEXPR(ir.arrayLayers > 0);
    ASSERT_CONSTEXPR(ir.dimensions < 3 || ir.arrayLayers == 1);
    ASSERT_CONSTEXPR(ir.frameswapCount > 0);
    ASSERT_CONSTEXPR(ir.mipLevels > 0);
    ASSERT_CONSTEXPR(ir.mipLevels == 1 || ir.dimensions > 1);
    ASSERT_CONSTEXPR((~ir.imageFlags & imageFlags_e::eCube) || (ir.arrayLayers % 6 == 0));
    ASSERT_CONSTEXPR((~ir.imageFlags & imageFlags_e::e3DIs2DArray) || (ir.dimensions == 3));
  };

  consteval imageRequirements& operator&=(imageRequirements& l, const imageRequirements& r) {
    if(l.format == vk::Format::eUndefined) l.format = r.format;
    ASSERT_CONSTEXPR(l.format == r.format || r.format == vk::Format::eUndefined);
    if(l.frameswapCount == 0) l.frameswapCount = r.frameswapCount;
    ASSERT_CONSTEXPR(l.frameswapCount == r.frameswapCount);
    if(l.mipLevels == 0) l.mipLevels = r.mipLevels;
    ASSERT_CONSTEXPR(l.mipLevels == r.mipLevels);
    if(l.deviceId == NONE) l.deviceId = r.deviceId;
    ASSERT_CONSTEXPR(l.deviceId == r.deviceId);
    if(!l.flow) l.flow = r.flow;
    if(min(l.dimensions, r.dimensions) == 2 && max(l.dimensions, r.dimensions) == 3) {
      //special case: a 3D image can be used as an array of 2D images. In no case may a 3D image have >1 array layer.
      l.dimensions = 3;
      l.arrayLayers = 1;
    } else if(l.dimensions == 0) {
      l.dimensions = r.dimensions;
    } else {
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

  consteval auto getImageFlow(uint64_t id, uint64_t onionId, const literalList<shader> shaders) {
    std::vector<imageRequirements::flowStep> ret;
    for(uint64_t sid = 0;sid < shaders.len;sid++) {
      vk::ImageLayout layout = vk::ImageLayout::eUndefined;
      vk::PipelineStageFlags2 stages;
      vk::AccessFlags2 access;
      const shader& s = shaders[sid];
      //MAYBE assert no buffer use? Or implicitly create a texel buffer?
      for(const shaderImageSlot& sis : s.sampled) {
	if(sis.id == id) {
	  if(layout != vk::ImageLayout::eGeneral)
	    layout = sis.writeStages ? vk::ImageLayout::eGeneral : vk::ImageLayout::eReadOnlyOptimal;
	  stages |= sis.readStages | sis.writeStages;
	  if(sis.writeStages) {
	    access |= vk::AccessFlagBits2::eShaderStorageWrite;
	    if(sis.readStages)
	      access |= vk::AccessFlagBits2::eShaderStorageRead;
	  } else
	    access |= vk::AccessFlagBits2::eShaderSampledRead;
	}
      }
      if(s.targetLink.depthStencil == id) {
	ASSERT_CONSTEXPR(layout == vk::ImageLayout::eUndefined);
	layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	stages |= vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
	access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
      }
      if(s.targetLink.color == id) {
	ASSERT_CONSTEXPR(layout == vk::ImageLayout::eUndefined);
	layout = vk::ImageLayout::eColorAttachmentOptimal;
	stages |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	access |= vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
      }
      if(layout == vk::ImageLayout::eUndefined) continue;
      if(ret.size() > 0 && ret[ret.size()-1].layout == layout) {
	ret[ret.size()-1].stages |= stages;
	continue;
      }
      ret.emplace_back(imageRequirements::flowStep{
	  .onionId = onionId,
	  .shaderId = sid,
	  .layout = layout,
	  .stages = stages,
	  .access = access
	});
    }
    return ret;
  };

  template<uint64_t id, uint64_t onionId, const literalList<shader> shaders>
  consteval size_t getImageFlowSize() {
    return getImageFlow(id, onionId, shaders).size();
  };

  template<uint64_t id, const literalList<shader> shaders, uint64_t gpuId>
  consteval imageRequirements getImageRequirements(literalList<imageRequirements::flowStep> flow) {
    imageRequirements ret {
      .deviceId = gpuId,
      .flow = flow
    };
    for(const shader& s : shaders) {
      for(const shaderImageSlot& sis : s.sampled)
	if(sis.id == id)
	  ret &= requirementsForSlot(sis);
      if(s.targetLink.depthStencil == id) ret.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
      if(s.targetLink.color == id) ret.usage |= vk::ImageUsageFlagBits::eColorAttachment;
    }
    return ret;
  };

  consteval auto getDefaultCI(imageRequirements r) {
    assertValid(r);
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

  consteval imageRequirements reflow(imageRequirements r, literalList<imageRequirements::flowStep> nflow) {
    r.flow = nflow;
    return r;
  };

}
