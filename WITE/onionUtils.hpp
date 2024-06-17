/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <vector>
#include <memory>

#include "onionTemplateStructs.hpp"
#include "math.hpp"
#include "descriptorPoolPool.hpp"
#include "hash.hpp"
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
      //logically should have an in and an out, some usages can do both. hostVisible can do both.
      && (((b.usage & (vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eStorageBuffer))
	   && (b.usage & (vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eUniformTexelBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)))
	  || b.hostVisible)
      && b.size
      && b.frameswapCount;
  };

  WITE_DEBUG_IB_CE vk::BufferCreateInfo getDefaultCI(bufferRequirements r) {
    ASSERT_CONSTEXPR(isValid(r));
    vk::BufferCreateInfo ret;
    ret.setSize(r.size);
    ret.setUsage(r.usage);
    return ret;
  };

  WITE_DEBUG_IB_CE vk::ImageCreateInfo getDefaultCI(imageRequirements r) {
    ASSERT_CONSTEXPR(isValid(r));
    vk::ImageCreateInfo ret { {}, (vk::ImageType)(r.dimensions-1), r.format, {}, r.mipLevels, r.arrayLayers,
      vk::SampleCountFlagBits::e1, r.hostVisible ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal, r.usage,
      vk::SharingMode::eExclusive };
    if(r.isCube) ret.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    if(r.dimensions == 3) ret.flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
    return ret;
  };

  consteval bufferRequirements stagingRequirementsFor(bufferRequirements r, uint8_t fc = 2) {
    bufferRequirements ret = r;
    ret.hostVisible = true;
    ret.usage = vk::BufferUsageFlagBits::eTransferSrc;
    ret.frameswapCount = fc;
    return ret;
  };

  template<class T> WITE_DEBUG_IB_CE size_t findId(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return i;
    constexprAssertFailed();
    return l.len;
  };

  template<class T> WITE_DEBUG_IB_CE bool containsId(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return true;
    return false;
  };

  template<class T> WITE_DEBUG_IB_CE auto findById(literalList<T> l, uint64_t id) {
    for(size_t i = 0;i < l.len;i++)
      if(l[i].id == id)
	return l[i];
    constexprAssertFailed();
    return l[0];
  };

  constexpr bool mightWrite(resourceConsumer usage) {
    return uint64_t(usage.access & (vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eShaderWrite));
  };

  template<const shaderModule* M, uint64_t GPUID> struct shaderFactory {
    static constexpr vk::ShaderModuleCreateInfo smci { {}, M->size, M->data };
    static constexpr vk::SpecializationInfo si { M->specializations.len, M->specializations.data, M->specializationDataSize, M->specializationData };
    static constexpr vk::PipelineShaderStageCreateInfo ssci { {}, M->stage, {}, M->entryPoint, &si };
    static void create(vk::PipelineShaderStageCreateInfo* out) {
      *out = ssci;
      VK_ASSERT(gpu::get(GPUID).getVkDevice().createShaderModule(&smci, ALLOCCB, &out->module), "Failed to create module");
    };
  };

  template<literalList<shaderModule> MS, uint64_t GPUID> void createModules(vk::PipelineShaderStageCreateInfo* out) {
    if constexpr(MS.len) {
      static constexpr shaderModule M = MS[0];
      shaderFactory<&M, GPUID>::create(out);
      createModules<MS.sub(1), GPUID>(out+1);
    }
  };

  template<literalList<shaderModule> MS, vk::ShaderStageFlagBits S> consteval bool containsStage() {
    for(const shaderModule& M : MS)
      if(M.stage == S)
	return true;
    return false;
  };

  enum class substep_e : uint8_t { barrier0, copy, barrier1, clear, barrier2, render, barrier3, compute, barrier4, post };

  struct resourceAccessTime {
    size_t layerIdx = NONE_size;
    substep_e substep;
    resourceConsumer usage;
    uint8_t frameLatency;
    size_t passIdx = 0;//for sorting only
    uint64_t passId = NONE;//forwarded to timing (NONE for compute shaders)
    size_t shaderIdx = 0;//for sorting only
    uint64_t shaderId = NONE;//forwarded to timing
    constexpr auto operator<=>(const resourceAccessTime& r) const {
      auto comp = frameLatency <=> r.frameLatency;
      if(comp != 0) return comp;
      comp = layerIdx <=> r.layerIdx;
      if(comp != 0) return comp;
      comp = substep <=> r.substep;
      if(comp != 0) return comp;
      comp = passIdx <=> r.passIdx;
      if(comp != 0) return comp;
      return shaderIdx <=> r.shaderIdx;
    };
    constexpr bool compatibleWith(const resourceAccessTime r) {
      //true means there need not be a barrier between them (with this coming before r)
      return !mightWrite(usage) && !mightWrite(r.usage) && frameLatency == r.frameLatency;
      //MAYBE should the layout for the given usage impact this?
      //not compatible if on different frameLatency so final usage per frame can be accurate
    };
  };

  struct resourceBarrierTiming {//used to key a map to ask what barrier(s) should happen at a given step, so frameLatency is not a factor
    size_t layerIdx;
    substep_e substep;
    uint64_t passId = NONE;
    uint64_t shaderId = NONE;
    constexpr auto operator<=>(const resourceBarrierTiming& r) const = default;
  };

  struct resourceBarrier {
    resourceBarrierTiming timing;
    uint8_t frameLatency = 0;
    resourceAccessTime before, after;
    uint64_t objectLayoutId, resourceSlotId, requirementId = NONE;
  };

  WITE_DEBUG_IB_CE resourceConsumer& operator|=(resourceConsumer& l, const resourceConsumer& r) {
    l.stages |= r.stages;
    l.access |= r.access;
    l.id = r.id;
    return l;
  };

  WITE_DEBUG_IB_CE resourceAccessTime& operator|=(resourceAccessTime&l, const resourceAccessTime& r) {
    l.layerIdx = r.layerIdx;
    l.substep = r.substep;
    l.usage |= r.usage;
    l.frameLatency = r.frameLatency;
    l.shaderIdx = r.shaderIdx;
    return l;
  };

  WITE_DEBUG_IB_CE bool isDepth(imageRequirements R) { return bool(R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment); };

  consteval vk::ImageSubresourceRange getAllInclusiveSubresource(const imageRequirements R) {
    // | vk::ImageAspectFlagBits::eStencil no stencil format has required support so stencil would need format cap detection
    return {
      /* .aspectMask =*/ isDepth(R) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor, //MAYBE multiplanar someday?
      /* .baseMipLevel =*/ 0,
      /* .levelCount =*/ R.mipLevels,
      /* .baseArrayLayer =*/ 0,
      /* .layerCount =*/ R.arrayLayers
    };
  };

  consteval vk::ImageSubresourceLayers getAllInclusiveSubresourceLayers(const imageRequirements R) {
    return {
      /* .aspectMask =*/ (R.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor, //MAYBE multiplanar someday?
      /* .baseMipLevel =*/ 0,
      /* .baseArrayLayer =*/ 0,
      /* .layerCount =*/ R.arrayLayers
    };
  };

  consteval vk::ImageSubresourceLayers toSubresourceLayers(const vk::ImageSubresourceRange R) {
    return {
      /* .aspectMask =*/ R.aspectMask,
      /* .baseMipLevel =*/ R.baseMipLevel,
      /* .baseArrayLayer =*/ R.baseArrayLayer,
      /* .layerCount =*/ R.layerCount
    };
  };

  template<imageRequirements R> consteval vk::ImageMemoryBarrier2 getBarrierSimple(vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    return { vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone, oldLayout, newLayout, 0, 0, {}, getAllInclusiveSubresource(R) };
  };

  WITE_DEBUG_IB_CE vk::ImageLayout imageLayoutFor(vk::AccessFlags2 access) {
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

  WITE_DEBUG_IB_CE vk::PipelineStageFlags2 toPipelineStage2(resourceAccessTime rat) {
    const auto f = rat.usage.stages;
    const auto a = rat.usage.access;
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
    if(a & (vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eTransferRead))
      ret |= vk::PipelineStageFlagBits2::eTransfer;
    return ret;
  };

  template<literalList<resourceConsumer> RRS, uint32_t binding = 0> consteval auto getBindingDescriptions() {
    if constexpr(RRS.len) {
      constexpr auto RR = RRS[0];
      constexpr copyableArray<vk::VertexInputBindingDescription, 1> ret = {{ binding, sizeofUdm<RR.usage.asVertex.format>(), RR.usage.asVertex.rate }};
      return concatArray(ret, getBindingDescriptions<RRS.sub(1), binding + 1>());
    } else {
      return copyableArray<vk::VertexInputBindingDescription, 0>();
    };
  };

  template<udm u, uint32_t binding, uint32_t idx, uint32_t locationOffset>
  consteval void getAttributeDescriptions(vk::VertexInputAttributeDescription* out) {
    if constexpr(idx < u.len) {
      out[idx] = { idx + locationOffset, binding, u[idx], idx ? 0 : sizeofUdm<u.sub(0, idx)>() };
      getAttributeDescriptions<u, binding, idx+1, locationOffset>(out);
    }
  };

  template<literalList<resourceConsumer> RRS, uint32_t binding = 0, uint32_t locationOffset = 0>
  consteval auto getAttributeDescriptions() {
    if constexpr(RRS.len) {
      constexpr resourceConsumer RR = RRS[0];
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
    uint64_t lastUpdatedFrame;
  };

  struct shaderInstance {
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;
  };

  struct perTargetLayoutPerShader {
    std::unique_ptr<descriptorPoolPoolBase> descriptorPool;
    std::map<uint64_t, shaderInstance> perSL;
    shaderInstance targetOnlyShader;
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
    shaderInstance sourceOnlyShader;
  };

  struct perSourceLayout {
    std::map<uint64_t, perSourceLayoutPerShader> perShader;
  };

  struct onionStaticData {
    static syncLock allDataMutex;
    static std::map<hash_t, onionStaticData> allOnionData;
    std::map<uint64_t, perTargetLayout> perTL;
    std::map<uint64_t, perSourceLayout> perSL;
    uint64_t objectIdSeed = 0;
  };

  //NOTE: A source/target layout is not allowed to name two resources referencing the same usage
  consteval const resourceReference* findResourceReferenceToSlot(literalList<resourceReference> RRS, uint64_t slotId) {
    if(slotId == NONE) return NULL;
    for(const resourceReference& RS : RRS)
      if(RS.resourceSlotId == slotId)
	return &RS;
    return NULL;
  };

  //NOTE: A source/target layout is not allowed to name two resources referencing the same usage
  consteval const resourceReference* findResourceReferenceToConsumer(literalList<resourceReference> RRS, uint64_t consumerId) {
    if(consumerId == NONE) return NULL;
    for(const resourceReference& RS : RRS)
      if(RS.resourceConsumerId == consumerId)
	return &RS;
    return NULL;
  };

  consteval resourceConsumer consumerForColorAttachment(uint64_t id) {
    return { id, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eColorAttachmentWrite };
  };

  consteval resourceConsumer consumerForDepthAttachment(uint64_t id) {
    return { id, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eDepthStencilAttachmentWrite };
  };

  consteval size_t calculateDescriptorCount(literalList<resourceConsumer> resources) {
    size_t ret = 0;
    for(const auto& rc : resources)
      if(rc.usage.type == resourceUsageType::eDescriptor)
	ret++;
    return ret;
  };

  template<literalList<resourceReference> resources> struct findVB {
    consteval bool operator()(const resourceConsumer& rr) {
      return rr.usage.type == resourceUsageType::eVertex &&
	rr.usage.asVertex.rate == vk::VertexInputRate::eVertex &&
	findResourceReferenceToConsumer(resources, rr.id);
    };
  };

  template<literalList<resourceReference> resources> struct findIB {
    consteval bool operator()(const resourceConsumer& rr) {
      return rr.usage.type == resourceUsageType::eVertex &&
	rr.usage.asVertex.rate == vk::VertexInputRate::eInstance &&
	findResourceReferenceToConsumer(resources, rr.id);
    };
  };

  template<literalList<resourceReference> resources> struct findIndex {
    consteval bool operator()(const resourceConsumer& rr) {
      return rr.usage.type == resourceUsageType::eIndex &&
	findResourceReferenceToConsumer(resources, rr.id);
    };
  };

  template<literalList<resourceReference> resources> struct findIndirect {
    consteval bool operator()(const resourceConsumer& rr) {
      return rr.usage.type == resourceUsageType::eIndirect &&
	findResourceReferenceToConsumer(resources, rr.id);
    };
  };

  template<literalList<resourceReference> resources> struct findIndirectCount {
    consteval bool operator()(const resourceConsumer& rr) {
      return rr.usage.type == resourceUsageType::eIndirectCount &&
	findResourceReferenceToConsumer(resources, rr.id);
    };
  };

  consteval uint32_t sizeofIndexType(vk::IndexType i) {
    switch(i) {
    case vk::IndexType::eUint16: return 2;
    case vk::IndexType::eUint32: return 4;
    case vk::IndexType::eUint8EXT: return 1;
    default: constexprAssertFailed(); return 0;
    }
  };

}
