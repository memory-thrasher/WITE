#pragma once

#include <vector>

#include "templateStructs.hpp"
#include "math.hpp"
#include "DEBUG.hpp"

namespace WITE {

  constexpr bool isValid(const imageRequirements& ir) {
    return (ir.dimensions > 1)
      && (ir.dimensions < 4)
      && (!ir.isCube || (ir.dimensions == 2 && ir.arrayLayers >= 6 && ir.arrayLayers % 6 == 0))
      && (ir.arrayLayers > 0)
      && (ir.dimensions < 3 || ir.arrayLayers == 1)
      && (ir.frameswapCount > 0)
      && (ir.mipLevels > 0)
      && (ir.flow)
      && (ir.frameswapCount > 0)
      && (ir.usage)
      && (ir.format != vk::Format::eUndefined)
      && (ir.deviceId != NONE);
  };

  constexpr bool isValid(const bufferRequirements& b) {
    return b.size && b.frameswapCount && b.deviceId != NONE;
  };

  consteval vk::BufferCreateInfo getDefaultCI(bufferRequirements r) {
    ASSERT_CONSTEXPR(isValid(r));
    vk::BufferCreateInfo ret;
    ret.setSize(r.size);
    ret.setUsage(r.usage);
    return ret;
  };

  consteval auto getDefaultCI(imageRequirements r) {
    ASSERT_CONSTEXPR(isValid(r));
    vk::ImageCreateInfo ret;
    ret.setImageType((vk::ImageType)(r.dimensions-1))//yes, I'm that lazy
      .setFormat(r.format)
      .setMipLevels(r.mipLevels)
      .setArrayLayers(r.arrayLayers)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setTiling(r.hostVisible ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal)
      .setUsage(r.usage)
      .setSharingMode(vk::SharingMode::eExclusive);
    if(r.isCube) ret.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    if(r.dimensions == 3) ret.flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
    return ret;
  };

  template<class T> consteval size_t findId(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return i;
    constexprAssertFailed();
    return l.len;
  };

  consteval vk::ImageSubresourceRange getAllInclusiveSubresource(const imageRequirements R) {
    return {
      /* .aspectMask =*/ (R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor, //MAYBE multiplanar someday?
      /* .baseMipLevel =*/ 0,
      /* .levelCount =*/ R.mipLevels,
      /* .baseArrayLayer =*/ 0,
      /* .layerCount =*/ R.arrayLayers
    };
  };

  template<imageRequirements R, literalList<imageFlowStep> IFS, literalList<shader> S, targetLayout TL>
  consteval copyableArray<vk::ImageMemoryBarrier2, R.flow.len> getDefaultFlowTransitions() {
    return [](size_t i){
      uint64_t srcId = R.flow[i],
	dstId = R.flow[i >= R.flow.len - 1 ? R.flowOverwrapIdx : i + 1];
      const auto& src = IFS[findId(IFS, srcId)],
	dst = IFS[findId(IFS, dstId)];
      vk::AccessFlags2 dstAccess, srcAccess;
      vk::PipelineStageFlags2 dstStages, srcStages;
      for(auto& s : S) {
	for(auto& mr : s.sourceProvidedResources) {
	  if(mr.requirementId == R.id && mr.flowId == srcId) {
	    srcAccess |= mr.access;
	    srcStages |= mr.readStages | mr.writeStages;
	  }
	  if(mr.requirementId == R.id && mr.flowId == dstId) {
	    dstAccess |= mr.access;
	    dstStages |= mr.readStages | mr.writeStages;
	  }
	}
      }
      for(auto& mr : TL.targetProvidedResources) {
	if(mr.requirementId == R.id && mr.flowId == srcId) {
	  srcAccess |= mr.access;
	  srcStages |= mr.readStages | mr.writeStages;
	}
	if(mr.requirementId == R.id && mr.flowId == dstId) {
	  dstAccess |= mr.access;
	  dstStages |= mr.readStages | mr.writeStages;
	}
      }
      vk::ImageMemoryBarrier2 ret;
      ret.setSrcStageMask(srcStages)
	.setSrcAccessMask(srcAccess)
	.setDstStageMask(dstStages)
	.setDstAccessMask(dstAccess)
	.setOldLayout(src.layout)
	.setNewLayout(dst.layout)
	.setSubresourceRange(getAllInclusiveSubresource(R));
      return ret;
    };
  };

}
