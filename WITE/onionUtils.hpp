#pragma once

#include <vector>
#include <memory>

#include "templateStructs.hpp"
#include "math.hpp"
#include "descriptorPoolPool.hpp"
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

  template<class T> constexpr bool containsId(literalList<T> l, uint64_t id) {
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
  constexpr const resourceMap* findResourceReferencing(literalList<resourceMap> RMS, uint64_t id) {
    for(const resourceMap& RM : RMS)
      for(uint64_t RR : RM.resourceReferences)
	if(RR == id)
	  return &RM;
    return NULL;
  };

  enum class substep_e : uint8_t { barrier1, copy, barrier2, render, barrier3, compute, barrier4 };

  struct resourceAccessTime {
    size_t layerIdx;
    substep_e substep;
    resourceReference usage;
    constexpr bool operator<(const resourceAccessTime& r) const {
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
    resourceAccessTime before, after;
    uint64_t layoutId, resourceId, requirementId;
  };

  consteval resourceReference& operator|=(resourceReference& l, const resourceReference& r) {
    l.stages |= r.stages;
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

  consteval vk::PipelineStageFlags2 toPipelineStage2(vk::ShaderStageFlags f) {
    if(f & vk::ShaderStageFlagBits::eAll)
      return vk::PipelineStageFlagBits2::eAllCommands;
    vk::PipelineStageFlags2 ret;
    if(f & vk::ShaderStageFlagBits::eAllGraphics)
      ret |= vk::PipelineStageFlagBits2::eAllGraphics;
    else {
      if(f & vk::ShaderStageFlagBits::eVertex)
	ret |= vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eVertexInput |
	  vk::PipelineStageFlagBits2::eVertexAttributeInput | vk::PipelineStageFlagBits2::eIndexInput;
      if(f & vk::ShaderStageFlagBits::eTessellationControl)
	ret |= vk::PipelineStageFlagBits2::eTessellationControlShader;
      if(f & vk::ShaderStageFlagBits::eTessellationEvaluation)
	ret |= vk::PipelineStageFlagBits2::eTessellationEvaluationShader;
      if(f & vk::ShaderStageFlagBits::eGeometry)
	ret |= vk::PipelineStageFlagBits2::eGeometryShader;
      if(f & vk::ShaderStageFlagBits::eFragment)
	ret |= vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eLateFragmentTests |
	  vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eColorAttachmentOutput;
      if(f & vk::ShaderStageFlagBits::eTaskEXT)
	ret |= vk::PipelineStageFlagBits2::eTaskShaderEXT;
      if(f & vk::ShaderStageFlagBits::eMeshEXT)
	ret |= vk::PipelineStageFlagBits2::eMeshShaderEXT;
    }
    if(f & vk::ShaderStageFlagBits::eCompute)
      ret |= vk::PipelineStageFlagBits2::eComputeShader;
    // if(f & vk::ShaderStageFlagBits::eRaygenKHR)
    //   ret |= vk::PipelineStageFlagBits2::eRaygenKHR;
    // if(f & vk::ShaderStageFlagBits::eAnyHitKHR)
    //   ret |= vk::PipelineStageFlagBits2::eAnyHitKHR;
    // if(f & vk::ShaderStageFlagBits::eClosestHitKHR)
    //   ret |= vk::PipelineStageFlagBits2::eClosestHitKHR;
    // if(f & vk::ShaderStageFlagBits::eMissKHR)
    //   ret |= vk::PipelineStageFlagBits2::eMissKHR;
    // if(f & vk::ShaderStageFlagBits::eIntersectionKHR)
    //   ret |= vk::PipelineStageFlagBits2::eIntersectionKHR;
    // if(f & vk::ShaderStageFlagBits::eCallableKHR)
    //   ret |= vk::PipelineStageFlagBits2::eCallableKHR;
    // if(f & vk::ShaderStageFlagBits::eRaygenNV)
    //   ret |= vk::PipelineStageFlagBits2::eRaygenNV;
    // if(f & vk::ShaderStageFlagBits::eAnyHitNV)
    //   ret |= vk::PipelineStageFlagBits2::eAnyHitNV;
    // if(f & vk::ShaderStageFlagBits::eClosestHitNV)
    //   ret |= vk::PipelineStageFlagBits2::eClosestHitNV;
    // if(f & vk::ShaderStageFlagBits::eMissNV)
    //   ret |= vk::PipelineStageFlagBits2::eMissNV;
    // if(f & vk::ShaderStageFlagBits::eIntersectionNV)
    //   ret |= vk::PipelineStageFlagBits2::eIntersectionNV;
    // if(f & vk::ShaderStageFlagBits::eCallableNV)
    //   ret |= vk::PipelineStageFlagBits2::eCallableNV;
    // if(f & vk::ShaderStageFlagBits::eSubpassShadingHUAWEI)
    //   ret |= vk::PipelineStageFlagBits2::eSubpassShadingHUAWEI;
    // if(f & vk::ShaderStageFlagBits::eClusterCullingHUAWEI)
    //   ret |= vk::PipelineStageFlagBits2::eClusterCullingHUAWEI;
    return ret;
  };

  template<literalList<resourceReference> RRS, uint32_t binding = 0> constexpr auto getBindingDescriptions() {
    if constexpr(RRS.len) {
      constexpr auto RR = RRS[0];
      constexpr copyableArray<vk::VertexInputBindingDescription, 1> ret = {{ binding, sizeofUdm<RR.usage.asVertex.format>(), RR.usage.asVertex.rate }};
      return concatArray(ret, getBindingDescriptions<RRS.sub(1), binding + 1>());
    } else {
      return copyableArray<vk::VertexInputBindingDescription, 0>();
    };
  };

  template<udm u, uint32_t binding, uint32_t idx, uint32_t locationOffset>
  constexpr void getAttributeDescriptions(vk::VertexInputAttributeDescription* out) {
    if constexpr(idx < u.len) {
      out[idx] = { idx + locationOffset, binding, u[idx], idx ? 0 : sizeofUdm<u.sub(0, idx)>() };
      getAttributeDescriptions<u, binding, idx+1, locationOffset>(out);
    }
  };

  template<literalList<resourceReference> RRS, uint32_t binding = 0, uint32_t locationOffset = 0>
  constexpr auto getAttributeDescriptions() {
    if constexpr(RRS.len) {
      constexpr resourceReference RR = RRS[0];
      copyableArray<vk::VertexInputAttributeDescription, RR.usage.asVertex.format.len> ret;
      getAttributeDescriptions<RR.usage.asVertex.format, binding, 0, locationOffset>(ret.ptr());
      return concatArray(ret, getAttributeDescriptions<RRS.sub(1), binding+1, locationOffset+RR.usage.asVertex.format.len>());
    } else {
      return copyableArray<vk::VertexInputAttributeDescription, 0>();
    }
  };

  struct frameBufferBundle {
    vk::Framebuffer fb;
    vk::ImageView attachments[2];
    vk::FramebufferCreateInfo fbci { {}, {}, 2, attachments };
  };

  struct perTargetLayoutPerSourceLayoutPerShader {
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;
  };

  struct perTargetLayoutPerShader {
    std::unique_ptr<descriptorPoolPoolBase> descriptorPool;
    std::map<uint64_t, perTargetLayoutPerSourceLayoutPerShader> perSL;
  };

  struct perTargetLayoutPerSourceLayout {
  };

  struct perTargetLayout {
    std::map<uint64_t, vk::RenderPass> rpByRequirementId;
    std::map<uint64_t, perTargetLayoutPerShader> perShader;
    std::map<uint64_t, perTargetLayoutPerSourceLayout> perSource;
  };

  struct perSourceLayoutPerShader {
    std::unique_ptr<descriptorPoolPoolBase> descriptorPool;
  };

  struct perSourceLayout {
    std::map<uint64_t, perSourceLayoutPerShader> perShader;
  };

  //if it's the same onion then it'll have the same pointers, no need to traverse into the lists
  constexpr hash_t hash(const onionDescriptor& od) {
    return hash(std::tie(od.IRS.len, od.IRS.data, od.BRS.len, od.BRS.data, od.CSRS.len, od.CSRS.data, od.RPRS.len, od.RPRS.data, od.CSS.len, od.CSS.data, od.LRS.len, od.LRS.data, od.TLS.len, od.TLS.data, od.SLS.len, od.SLS.data));
  };

  struct onionStaticData {
    static syncLock allDataMutex;
    static std::map<hash_t, onionStaticData> allOnionData;
    std::map<uint64_t, perTargetLayout> perTL;
    std::map<uint64_t, perSourceLayout> perSL;
  };

}
