#pragma once

#include <vector>

#include "templateStructs.hpp"
#include "math.hpp"
#include "DEBUG.hpp"

namespace WITE {

  constexpr bool isValid(const imageRequirements& ir) {
    return (ir.deviceId != NONE)
      && (ir.format != vk::Format::eUndefined)
      //logically usage should have an in and an out, some can do both
      && (ir.usage & (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment))
      && (ir.usage & (vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment))
      && (ir.dimensions > 1)
      && (ir.dimensions < 4)
      && (ir.frameswapCount > 0)
      && (!ir.isCube || (ir.dimensions == 2 && ir.arrayLayers >= 6 && ir.arrayLayers % 6 == 0))
      && (ir.arrayLayers > 0)
      && (ir.dimensions < 3 || ir.arrayLayers == 1)
      && (ir.mipLevels > 0);
  };

  constexpr bool isValid(const bufferRequirements& b) {
    return b.deviceId != NONE
      //logically should have an in and an out, some can do both
      && (b.usage & (vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eStorageBuffer))
      && (b.usage & (vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eUniformTexelBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer))
      && b.size
      && b.frameswapCount;
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

  template<class T> consteval bool containsId(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return true;
    return false;
  };

  template<class T> consteval auto findById(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return l[i];
    constexprAssertFailed();
    return l[0];
  };

  //NOTE: A source/target layout is not allowed to name two resources referencing the same usage
  consteval const resourceMap* findResourceReferencing(literalList<resourceMap> RMS, uint64_t id) {
    for(const resourceMap& RM : RMS)
      for(uint64_t RR : RM.resourceReferences)
	if(RR == id)
	  return &RM;
    return NULL;
  };

  enum class substep_e : uint8_t { barrier1, copy, barrier2, render, barrier3, compute, barrier4 };

  struct resourceUsage {
    size_t layerIdx;
    substep_e substep;
    resourceReference usage;
    constexpr bool operator<(const resourceUsage& r) const {
      return usage.frameLatency < r.usage.frameLatency || (usage.frameLatency == r.usage.frameLatency &&
							   (layerIdx < r.layerIdx || (layerIdx == r.layerIdx &&
										      substep < r.substep)));
    };
  };

  struct resourceBarrierTiming {//used to key a map to ask what barrier(s) should happen at a given step, so frameLatency is not a factor
    size_t layerIdx;
    substep_e substep;
    constexpr auto operator<=>(const resourceBarrierTiming& r) const = default;
  };

  struct resourceBarrier {
    resourceBarrierTiming timing;
    uint8_t frameLatency = 0;
    resourceUsage before, after;
    uint64_t layoutId, resourceId, requirementId;
  };

  consteval resourceReference& operator|=(resourceReference& l, const resourceReference& r) {
    l.readStages |= r.readStages;
    l.writeStages |= r.writeStages;
    l.access |= r.access;
    l.id = r.id;
    l.frameLatency = r.frameLatency;
    return l;
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

  consteval vk::ImageSubresourceLayers getAllInclusiveSubresourceLayers(const imageRequirements R) {
    return {
      /* .aspectMask =*/ (R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eColor, //MAYBE multiplanar someday?
      /* .baseMipLevel =*/ 0,
      /* .baseArrayLayer =*/ 0,
      /* .layerCount =*/ R.arrayLayers
    };
  };

  consteval vk::ImageLayout imageLayoutFor(vk::AccessFlags2 access) {
    vk::ImageLayout ret = vk::ImageLayout::eUndefined;
    if(access & (vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eIndexRead | vk::AccessFlagBits2::eVertexAttributeRead | vk::AccessFlagBits2::eUniformRead | vk::AccessFlagBits2::eInputAttachmentRead | vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderSampledRead | vk::AccessFlagBits2::eShaderStorageRead))
      ret = vk::ImageLayout::eShaderReadOnlyOptimal;
    if(access & (vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eHostRead | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eShaderStorageWrite))
      ret = vk::ImageLayout::eGeneral;
    if(access & (vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead))
      ret = ret == vk::ImageLayout::eUndefined ? vk::ImageLayout::eColorAttachmentOptimal : vk::ImageLayout::eGeneral;
    if(access & (vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead))
      ret = ret == vk::ImageLayout::eUndefined ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eGeneral;
    if(access & vk::AccessFlagBits2::eTransferRead)
      ret = ret == vk::ImageLayout::eUndefined ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eGeneral;
    if(access & vk::AccessFlagBits2::eTransferWrite)
      ret = ret == vk::ImageLayout::eUndefined ? vk::ImageLayout::eTransferDstOptimal : vk::ImageLayout::eGeneral;
    return ret;
  };

  consteval vk::DescriptorType descriptorTypeForAccess(vk::AccessFlags2 access, bool isImage) {
    if(access & vk::AccessFlagBits2::eShaderSampledRead)
      return vk::DescriptorType::eSampler;
    if(access & (vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite))
      return isImage ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eStorageBuffer;
    // if(access & vk::AccessFlagBits2::eUniformRead)
    return vk::DescriptorType::eUniformBuffer;
  };

  struct frameBufferBundle {
    vk::Framebuffer fb;
    vk::ImageView attachments[2];
    vk::FramebufferCreateInfo fbci { {}, {}, 2, attachments };
  };

  struct perTargetLayoutPerSourceLayout {
    //TODO graphics pipeline
    //TODO compute pipeline
    //TODO pipeline layout
  };

  struct perTargetLayoutPerShader {
    vk::DescriptorSetLayout descriptorSetLayout;
  };

  struct perTargetLayout {
    std::map<uint64_t, vk::RenderPass> rpByRequirementId;
    std::map<sourceLayout, perTargetLayoutPerSourceLayout> perSL;
    std::map<uint64_t, perTargetLayoutPerShader> perShader;
  };

  struct perSourceLayoutPerShader {
    vk::DescriptorSetLayout descriptorSetLayout;
  };

  struct perSourceLayout {
    std::map<uint64_t, perSourceLayoutPerShader> perShader;
  };

  struct onionStaticData {
    static syncLock allDataMutex;
    static std::map<onionDescriptor, onionStaticData> allOnionData;
    std::map<targetLayout, perTargetLayout> perTL;
    std::map<sourceLayout, perSourceLayout> perSL;
  };

}
