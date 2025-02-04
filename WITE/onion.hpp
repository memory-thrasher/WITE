/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <numeric>
#include <type_traits>

#include "window.hpp"
#include "onionTemplateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "iterableRecyclingPool.hpp"
#include "descriptorPoolPool.hpp"
#include "tempMap.hpp"
#include "profiler.hpp"

namespace WITE {

  template<onionDescriptor OD> struct onion {

    static consteval size_t frameswapLCM(literalList<resourceReference> rms) {
      size_t ret = 1;
      for(const resourceReference& rm : rms) {
	resourceSlot rs = findById(OD.RSS, rm.resourceSlotId);
	ret = std::lcm(ret, containsId(OD.IRS, rs.requirementId) ?
		       findById(OD.IRS, rs.requirementId).frameswapCount :
		       findById(OD.BRS, rs.requirementId).frameswapCount);
      }
      return ret;
    };

    static consteval size_t frameswapLCMAll() {
      size_t ret = 1;
      for(const auto& IR : OD.IRS)
	ret = std::lcm(ret, IR.frameswapCount);
      for(const auto& BR : OD.BRS)
	ret = std::lcm(ret, BR.frameswapCount);
      return ret;
    };

    static constexpr size_t cmdFrameswapCount = max(3, frameswapLCMAll());

    static constexpr auto allTargetIds = map<targetLayout, uint64_t, OD.TLS>([](const targetLayout& l){ return l.id; });
    static constexpr auto allSourceIds = map<sourceLayout, uint64_t, OD.SLS>([](const sourceLayout& l){ return l.id; });
    static constexpr auto allLayoutIds = concat<uint64_t, allTargetIds, allSourceIds>();
    static constexpr size_t layerIdx_OPTE = OD.LRS.len-1;

    uint64_t frame = 1;//can't signal a timeline semaphore with 0
    syncLock mutex;
    vk::CommandPool cmdPool;
    gpu* dev;
    vk::CommandBuffer primaryCmds[cmdFrameswapCount];
    vk::Fence fences[cmdFrameswapCount];
    vk::Semaphore semaphore;
    copyableArray<garbageCollector, cmdFrameswapCount> collectors { OD.GPUID };
    onionStaticData& od;

    static onionStaticData& getOd() {
      PROFILEME;
      scopeLock lock(&onionStaticData::allDataMutex);
      return onionStaticData::allOnionData[hash(OD)];
    };

    onion() : od(getOd()) {
      PROFILEME;
      dev = &gpu::get(OD.GPUID);//note: this has a modulo op so GPUID does not need be < num of gpus on the system
      const vk::CommandPoolCreateInfo poolCI(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, dev->getQueueFam());
      VK_ASSERT(dev->getVkDevice().createCommandPool(&poolCI, ALLOCCB, &cmdPool), "failed to allocate command pool");
      vk::CommandBufferAllocateInfo allocInfo(cmdPool, vk::CommandBufferLevel::ePrimary, cmdFrameswapCount);
      VK_ASSERT(dev->getVkDevice().allocateCommandBuffers(&allocInfo, primaryCmds), "failed to allocate command buffer");
      static constexpr vk::FenceCreateInfo fenceCI(vk::FenceCreateFlagBits::eSignaled);
      for(size_t i = 0;i < cmdFrameswapCount;i++)
	VK_ASSERT(dev->getVkDevice().createFence(&fenceCI, ALLOCCB, &fences[i]), "failed to create fence");
      vk::SemaphoreTypeCreateInfo semTypeCI(vk::SemaphoreType::eTimeline);
      vk::SemaphoreCreateInfo semCI({}, &semTypeCI);
      VK_ASSERT(dev->getVkDevice().createSemaphore(&semCI, ALLOCCB, &semaphore), "failed to create semaphore");
#ifdef WITE_DEBUG_IMAGE_BARRIERS
      WARN("In image barrier debug mode, dumping image barrier information");
      for(const auto& mb : allBarriers()) {
	WARN("barrier for resource: objectLayoutId: ", mb.objectLayoutId, ", resourceSlotId: ", mb.resourceSlotId, ", requirementId: ", mb.requirementId, ", at frame latency: ", (int)mb.timing.frameLatency, " will be transitioned from access: ", std::hex, (uint64_t)mb.before.access, std::dec, " (layout ", (int)mb.beforeLayout.layout, ") to access ", std::hex, (uint64_t)mb.after.access, std::dec, " (layout ", (int)mb.afterLayout.layout, ") on: layerIdx: ", mb.timing.barrierId.layerIdx, ", substep: ", (int)mb.timing.barrierId.substep, ", pass/shader idx: ", mb.timing.barrierId.passOrShaderIdx);
      }
#endif
    };

    inline garbageCollector& getActiveGarbageCollector() {
      PROFILEME;
      return collectors[frame % cmdFrameswapCount];
    };

    static consteval vk::ImageSubresourceRange getSubresource(const uint64_t IRID, const resourceReference& RR) {
      if(!RR.subresource.isDefault)
	return RR.subresource.range;
      else
	return getAllInclusiveSubresource(findById(OD.IRS, IRID));
    };

    static consteval bool resourceSlotConsumedBy(uint64_t rsId, uint64_t consId) {
      for(const targetLayout& tl : OD.TLS)
	for(const resourceReference& rr : tl.resources)
	  if(rr.resourceSlotId == rsId && rr.resourceConsumerId == consId)
	    return true;
      for(const sourceLayout& sl : OD.SLS)
	for(const resourceReference& rr : sl.resources)
	  if(rr.resourceSlotId == rsId && rr.resourceConsumerId == consId)
	    return true;
      return false;
    };

    static consteval resourceConsumer consumerForWindowForObject(uint64_t objectLayoutId) {
      objectLayout OL = findById(OD.OLS, objectLayoutId);
      return { OL.windowConsumerId, {}, vk::AccessFlagBits2::eTransferRead, {} };
    };

    template<uint64_t id> static consteval auto findShader() {
      if constexpr(containsId(OD.CSRS, id)) {
	return findById(OD.CSRS, id);
      } else {
	for(const auto& RP : OD.RPRS)
	  if(containsId(RP.shaders, id))
	    return findById(RP.shaders, id);
	constexprAssertFailed();
	return graphicsShaderRequirements();//impossible but compiler whines if not here
      }
    };

    static consteval bool satisfies(literalList<resourceReference> resources, literalList<resourceConsumer> consumers) {
      for(resourceConsumer rc : consumers)
	if(!findResourceReferenceToConsumer(resources, rc.id))
	  return false;
      return true;
    };

    static consteval bool satisfies(literalList<resourceReference> resources, literalList<uint64_t> consumers) {
      for(uint64_t rc : consumers)
	if(!findResourceReferenceToConsumer(resources, rc))
	  return false;
      return true;
    };

    static WITE_DEBUG_IB_CE std::vector<resourceAccessTime> findUsages(uint64_t resourceSlotId) {
      std::vector<resourceAccessTime> ret;
      tempMap<uint64_t, resourceReference> referencesByConsumerId;
      // const resourceSlot RS = findById(OD.RSS, resourceSlotId);
      for(const targetLayout& tl : OD.TLS)
	for(const resourceReference& rr : tl.resources)
	  if(rr.resourceSlotId == resourceSlotId)
	    referencesByConsumerId[rr.resourceConsumerId] = rr;
      for(const sourceLayout& sl : OD.SLS)
	for(const resourceReference& rr : sl.resources)
	  if(rr.resourceSlotId == resourceSlotId)
	    referencesByConsumerId[rr.resourceConsumerId] = rr;
      for(size_t layerIdx = 0;layerIdx < OD.LRS.len;layerIdx++) {
	const layerRequirements& layer = OD.LRS[layerIdx];
	//copy:
	for(uint64_t substepId : layer.copies) {
	  const auto& substep = findById(OD.CSS, substepId);
	  if(referencesByConsumerId.contains(substep.src))
	    ret.push_back({ layerIdx, substep_e::copy, {}, vk::AccessFlagBits2::eTransferRead, referencesByConsumerId[substep.src].frameLatency });
	  if(referencesByConsumerId.contains(substep.dst))
	    ret.push_back({ layerIdx, substep_e::copy, {}, vk::AccessFlagBits2::eTransferWrite, referencesByConsumerId[substep.dst].frameLatency });
	}
	//clear:
	for(uint64_t substepId : layer.clears) {
	  const auto& substep = findById(OD.CLS, substepId);
	  if(referencesByConsumerId.contains(substep.id))
	    ret.push_back({ layerIdx, substep_e::clear, {}, vk::AccessFlagBits2::eTransferWrite, referencesByConsumerId[substep.id].frameLatency });
	}
	//rendering:
	static_assert(std::numeric_limits<decltype(resourceInstanceBarrierTiming::frameLatency)>::min() == 0);
	for(uint64_t passIdx = 0;passIdx < layer.renders.len;passIdx++) {
	  uint64_t passId = layer.renders[passIdx];
	  const auto& pass = findById(OD.RPRS, passId);
	  if(referencesByConsumerId.contains(pass.depth))
	    ret.push_back({ layerIdx, substep_e::render, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead, referencesByConsumerId[pass.depth].frameLatency, passIdx, passId });
	  for(uint64_t color : pass.color)
	    if(referencesByConsumerId.contains(color))
	      ret.push_back({ layerIdx, substep_e::render, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead, referencesByConsumerId[color].frameLatency, passIdx, passId });
	  for(uint64_t input : pass.input)
	    if(referencesByConsumerId.contains(input))
	      ret.push_back({ layerIdx, substep_e::render, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eInputAttachmentRead, referencesByConsumerId[input].frameLatency, passIdx, passId });
	  for(const graphicsShaderRequirements& gsr : pass.shaders) {
	    //NOTE: graphical shaders use shader id of 0 because all shaders in a render pass are considered concurrent.
	    for(const resourceConsumer& srr : gsr.targetProvidedResources)
	      if(referencesByConsumerId.contains(srr.id))
		ret.push_back({ layerIdx, substep_e::render, srr.stages, srr.access, referencesByConsumerId[srr.id].frameLatency, passIdx, passId });
	    for(const resourceConsumer& srr : gsr.sourceProvidedResources)
	      if(referencesByConsumerId.contains(srr.id))
		ret.push_back({ layerIdx, substep_e::render, srr.stages, srr.access, referencesByConsumerId[srr.id].frameLatency, passIdx, passId });
	  }
	}
	//compute:
	size_t shaderIdx = 0;
	for(uint64_t csrId : layer.computeShaders) {
	  const computeShaderRequirements& csr = findById(OD.CSRS, csrId);
	  for(const resourceConsumer& srr : csr.targetProvidedResources)
	    if(referencesByConsumerId.contains(srr.id))
	      ret.push_back({ layerIdx, substep_e::compute, srr.stages, srr.access, referencesByConsumerId[srr.id].frameLatency, shaderIdx, csr.id });
	  for(const resourceConsumer& srr : csr.sourceProvidedResources)
	    if(referencesByConsumerId.contains(srr.id))
	      ret.push_back({ layerIdx, substep_e::compute, srr.stages, srr.access, referencesByConsumerId[srr.id].frameLatency, shaderIdx, csr.id });
	  shaderIdx++;
	}
      }
      for(auto& ol : OD.OLS)
	if(referencesByConsumerId.contains(ol.windowConsumerId))
	  ret.push_back({ OD.LRS.len-1, substep_e::post, {}, vk::AccessFlagBits2::eTransferRead, referencesByConsumerId[ol.windowConsumerId].frameLatency });
      std::sort(ret.begin(), ret.end());
      //now combine any simultaneous groups (which will now be contiguous)
      size_t removed = 0;
      for(size_t i = 1, j = 0;i < ret.size();i++) {
	if(ret[i] == ret[j]) {//compares timing not access
	  ret[j].stages |= ret[i].stages;
	  ret[j].access |= ret[i].access;
	  ++removed;
	} else {
	  ++j;
	  if(i != j)
	    ret[j] = ret[i];
	}
      }
      ret.resize(ret.size() - removed);
      return ret;
    };

    static WITE_DEBUG_IB_CE std::vector<resourceBarrier> findBarriers(uint64_t resourceSlotId) {
      auto usage = findUsages(resourceSlotId);
      constexpr_assert(usage.size() > 0);//all resources should be used at least once
      size_t afterIdx = 0;
      std::vector<resourceBarrier> ret;
      resourceAccessTime before = usage[usage.size()-1];
      resourceSlot RS = findById(OD.RSS, resourceSlotId);
      while(afterIdx < usage.size()) {
	resourceBarrier rb;
	rb.before = before;
	rb.after = usage[afterIdx];
	rb.resourceSlotId = RS.id;
	rb.requirementId = RS.requirementId;
	rb.objectLayoutId = RS.objectLayoutId;
	rb.timing.barrierId.layerIdx = rb.after.layerIdx;
	rb.timing.frameLatency = rb.after.frameLatency;
	if(rb.after.frameLatency != rb.before.frameLatency || //after takes place on a later frame
	   rb.before.layerIdx != rb.after.layerIdx ||
	   afterIdx == 0) { //afterIdx == 0: edge case for only having 1 layer
	  //prefer top of layer for barriers.
	  rb.timing.barrierId.substep = substep_e::barrier0;
	} else if(rb.after.substep != rb.before.substep) {//next prefer between steps
	  rb.timing.barrierId.substep = (substep_e)((int)rb.after.substep - 1);
	} else if(rb.after.passOrShaderId != rb.before.passOrShaderId) {//barriers between render passes are also allowed
	  constexpr_assert(rb.after.substep == substep_e::render);//only render has passes
	  rb.timing.barrierId.substep = rb.after.substep;
	  rb.timing.barrierId.passOrShaderIdx = rb.after.passOrShaderIdx;
	} else {//otherwise wait as long as possible
	  constexpr_assert(rb.after.substep == substep_e::compute);//mid-pass barriers not supported (would need dependencies)
	  //also, multiple target layouts would execute the same barrier repeatedly
	  rb.timing.barrierId.substep = rb.after.substep;
	  rb.timing.barrierId.passOrShaderIdx = rb.after.passOrShaderIdx;
	}
	//having used the timing for the initial "after" value to decide when to drop the barrier, we can now absorb future usages that don't need a barrier after this one
	afterIdx++;
	while(afterIdx < usage.size() && rb.after.compatibleWith(usage[afterIdx])) {
	  rb.after |= usage[afterIdx];
	  afterIdx++;
	}
	ret.push_back(rb);
	before = rb.after;
      }
      ret[0].before = before;
      if(containsId(OD.IRS, RS.requirementId)) {
	auto IR = findById(OD.IRS, RS.requirementId);
	for(auto& r : ret) {
	  r.beforeLayout = bestImageLayoutFor(r.before.access, IR.usage);
	  r.afterLayout = bestImageLayoutFor(r.after.access, IR.usage);
	}
      }
      return ret;
    };

    template<uint64_t resourceSlotId, imageRequirements IR>
    static consteval copyableArray<resourceBarrier, IR.frameswapCount> findFinalBarrierPerFrame() {
      auto barriers = findBarriers(resourceSlotId);
      constexpr_assert(barriers.size() > 0);
      copyableArray<resourceBarrier, IR.frameswapCount> ret;
      for(auto& b : barriers)
	ret[b.timing.frameLatency] = b;
      for(size_t i = 0;i < ret.LENGTH;i++) {
	auto& b = ret[i];
	if(b.after.layerIdx == NONE_size) //empty frame slot when default initialized
	  for(size_t j = ret.LENGTH;j && b.after.layerIdx == NONE_size;j--)
	    b = ret[(i + j - 1) % ret.LENGTH];
	constexpr_assert(b.timing.barrierId.layerIdx != NONE_size);//image never used, so can't pick which layout to init to
	//MAYBE add flag to not init? But why would it be in the onion if not used
      }
      return ret;
    };

    static consteval layoutAccessMatrix_t getLayoutForImage(uint64_t resourceSlotId, resourceInstanceBarrierTiming BT) {
      auto barriers = findBarriers(resourceSlotId);
      constexpr_assert(barriers.size() > 0);
      if(barriers.size() == 1 || barriers[0].timing > BT)
	return barriers[0].beforeLayout;
      size_t i = 1;
      while(i < barriers.size() && barriers[i].timing < BT)
	i++;
      return barriers[i-1].afterLayout;
    };

    template<uint64_t objectLayoutId>
    static consteval auto getResourceReferences_v() {
      std::vector<resourceReference> ret;
      for(const targetLayout& tl : OD.TLS)
	if(tl.objectLayoutId == objectLayoutId)
	  for(const resourceReference& rr : tl.resources)
	    ret.push_back(rr);
      for(const sourceLayout& sl : OD.SLS)
	if(sl.objectLayoutId == objectLayoutId)
	  for(const resourceReference& rr : sl.resources)
	    ret.push_back(rr);
      return ret;
    };

    template<uint64_t objectLayoutId>
    static consteval auto getResourceReferences_c() {
      return getResourceReferences_v<objectLayoutId>().size();
    };

    template<uint64_t objectLayoutId>
    static consteval copyableArray<resourceReference, getResourceReferences_c<objectLayoutId>()> getResourceReferences() {
      return getResourceReferences_v<objectLayoutId>();
    };

    template<uint64_t objectLayoutId>
    static consteval auto getResourceSlots_v() {
      return OD.RSS.where([](const resourceSlot& rs)->bool{ return rs.objectLayoutId == objectLayoutId; });
    };

    template<uint64_t objectLayoutId>
    static consteval auto getResourceSlots_c() {
      return getResourceSlots_v<objectLayoutId>().size();
    };

    template<uint64_t objectLayoutId>
    static consteval copyableArray<resourceSlot, getResourceSlots_c<objectLayoutId>()> getResourceSlots() {
      return getResourceSlots_v<objectLayoutId>();
    };

    template<uint64_t objectLayoutId>
    static consteval auto getSourceLayouts_v() {
      return OD.SLS.where([](const sourceLayout& rs)->bool{ return rs.objectLayoutId == objectLayoutId; });
    };

    template<uint64_t objectLayoutId>
    static consteval auto getSourceLayouts_c() {
      return getSourceLayouts_v<objectLayoutId>().size();
    };

    template<uint64_t objectLayoutId>
    static consteval copyableArray<sourceLayout, getSourceLayouts_c<objectLayoutId>()> getSourceLayouts() {
      return getSourceLayouts_v<objectLayoutId>();
    };

    template<uint64_t objectLayoutId>
    static consteval auto getTargetLayouts_v() {
      return OD.TLS.where([](const targetLayout& rs)->bool{ return rs.objectLayoutId == objectLayoutId; });
    };

    template<uint64_t objectLayoutId>
    static consteval auto getTargetLayouts_c() {
      return getTargetLayouts_v<objectLayoutId>().size();
    };

    template<uint64_t objectLayoutId>
    static consteval copyableArray<targetLayout, getTargetLayouts_c<objectLayoutId>()> getTargetLayouts() {
      return getTargetLayouts_v<objectLayoutId>();
    };

    template<resourceSlot> struct resourceTraits : std::false_type {};

    template<resourceSlot RS> requires(!RS.external && containsId(OD.IRS, RS.requirementId)) struct resourceTraits<RS> {
      static constexpr imageRequirements IR = findById(OD.IRS, RS.requirementId);
      static constexpr bool isImage = true;
      typedef image<IR> type;
    };

    template<resourceSlot RS> requires(!RS.external && containsId(OD.BRS, RS.requirementId)) struct resourceTraits<RS> {
      static constexpr bufferRequirements BR = findById(OD.BRS, RS.requirementId);
      static constexpr bool isImage = false;
      typedef buffer<BR> type;
    };

    template<resourceSlot RS> requires(RS.external && containsId(OD.IRS, RS.requirementId)) struct resourceTraits<RS> {
      static constexpr imageRequirements IR = findById(OD.IRS, RS.requirementId);
      static constexpr bool isImage = true;
      typedef image<IR>* type;
    };

    template<resourceSlot RS> requires(RS.external && containsId(OD.BRS, RS.requirementId)) struct resourceTraits<RS> {
      static constexpr bufferRequirements BR = findById(OD.BRS, RS.requirementId);
      static constexpr bool isImage = false;
      typedef buffer<BR>* type;
    };

    template<uint64_t objectLayoutId, size_t idx = 0> struct mappedResourceTuple {
      static constexpr auto RSS = getResourceSlots<objectLayoutId>();
      static constexpr resourceSlot RS = RSS[idx];
      typedef resourceTraits<RS> RT;
      RT::type data;
      mappedResourceTuple<objectLayoutId, idx+1> rest;
      uint64_t frameLastUpdatedThisExternal = 0;

      mappedResourceTuple() {
	if constexpr(RS.external)
	  data = NULL;
#ifdef WITE_DEBUG_IMAGE_BARRIERS
	if constexpr(RT::isImage)
	  for(size_t i = 0;i < RT::IR.frameswapCount;i++)
	    WARN("Created image with handle ", std::hex, at().frameImage(i), std::dec, " for resource slot ", RS.id);
#endif
      };

      template<size_t RSID = RS.id> inline auto& at() {
	PROFILEME;
	if constexpr(RSID == RS.id) {
	  if constexpr(RS.external) {
	    ASSERT_TRAP(data, "External resource not set before access OL: ", objectLayoutId, " ID: ", RSID);
	    return *data;
	  } else {
	    return data;
	  }
	} else {
	  return rest.template at<RSID>();
	}
      };

      template<size_t RSID = RS.id> inline void set(auto* t, uint64_t currentFrame) {
	PROFILEME;
	if constexpr(RSID == RS.id) {
	  static_assert(RS.external);
	  if(data != t) {
	    data = t;
	    frameLastUpdatedThisExternal = currentFrame;
	  }
	} else {
	  rest.template set<RSID>(t, currentFrame);
	}
      };

      template<size_t RSID = RS.id> inline uint64_t frameLastUpdatedExternal() {
	PROFILEME;
	if constexpr(RSID == RS.id) {
	  return frameLastUpdatedThisExternal;
	} else {
	  return rest.template frameLastUpdatedExternal<RSID>();
	}
      };

      inline uint64_t frameLastUpdated(uint64_t currentFrame) {
	PROFILEME;
	uint64_t ret = max(at().frameUpdated(currentFrame), frameLastUpdatedThisExternal);
	if constexpr(idx < RSS.len - 1) {
	  ret = max(ret, rest.frameLastUpdated(currentFrame));
	}
	return ret;
      };

      template<uint64_t FS> inline void preRender_l2(uint64_t frame, vk::CommandBuffer cmd, garbageCollector& gc) {
	PROFILEME;
	if constexpr(RT::isImage) {
	  static constexpr auto RB = findFinalBarrierPerFrame<RS.id, RT::IR>()[FS];
	  at().template applyPendingResize<RS.resizeBehavior, RB.afterLayout.layout>(frame + FS, cmd, gc);
	  if constexpr(FS < RT::IR.frameswapCount-1)
	    preRender_l2<FS+1>(frame, cmd, gc);
	}
      }

      inline void preRender(uint64_t frame, vk::CommandBuffer cmd, garbageCollector& gc) {
	PROFILEME;
	if constexpr(RT::isImage)
	  preRender_l2<0>(frame, cmd, gc);
	if constexpr(idx < RSS.len - 1)
	  rest.preRender(frame, cmd, gc);
      };

      inline void trackWindowSize(uint64_t frame, vk::Extent3D& wSize) {
	PROFILEME;
	if constexpr(RT::isImage && RS.resizeBehavior.image.trackWindow) {
	  auto& img = at();
	  auto imgSize = img.getSizeExtent(frame);
	  if(wSize != imgSize)
	    img.resize(frame, wSize);
	}
	if constexpr(idx < RSS.len - 1)
	  rest.trackWindowSize(frame, wSize);
      };

      inline static consteval auto initBaselineBarriers() {
	//consteval lambdas are finicky, need to be wrapped in a consteval function
	if constexpr(RT::isImage) {
	  constexpr auto finalUsagePerFrame = findFinalBarrierPerFrame<RS.id, RT::IR>();
	  return copyableArray<vk::ImageMemoryBarrier2, RT::IR.frameswapCount>([&](size_t i) consteval {
	    return vk::ImageMemoryBarrier2 {
	      vk::PipelineStageFlagBits2::eNone,
	      vk::AccessFlagBits2::eNone,
	      toPipelineStage2(finalUsagePerFrame[i].after),
	      finalUsagePerFrame[i].after.access,
	      vk::ImageLayout::eUndefined,
	      finalUsagePerFrame[i].afterLayout.layout,
	      {}, {}, {},
	      getAllInclusiveSubresource(RT::IR)
	    };
	  });
	} else {
	  constexprAssertFailed();//this function is only for images
	}
      };

      inline void init(uint64_t currentFrame, vk::CommandBuffer cmd) {
	if constexpr(RT::isImage && !RS.external) {
	  PROFILEME;
	  //only images need a layout initialization, each into the layouts they will be assumed to be when first used.
	  static constexpr size_t FC = RT::IR.frameswapCount;
	  static constexpr auto baseline = initBaselineBarriers();
	  copyableArray<vk::ImageMemoryBarrier2, FC> barriers = baseline;
	  for(int64_t i = 0;i < FC;i++) {
	    barriers[i].image = data.frameImage(i + currentFrame - 1);
	    //-1: starting layout for this frame = the final layouy for the previous frame
	    WITE_DEBUG_IB(barriers[i], cmd);
	  }
	  vk::DependencyInfo di { {}, 0, NULL, 0, NULL, FC, barriers.ptr() };
	  cmd.pipelineBarrier2(&di);
	}
	if constexpr(idx < RSS.len - 1)
	  rest.init(currentFrame, cmd);
      };

    };

    template<uint64_t objectLayoutId, size_t idx> requires(getResourceSlots_c<objectLayoutId>() == idx)
      struct mappedResourceTuple<objectLayoutId, idx> : std::false_type {};

    template<literalList<resourceReference> resources>
    static consteval auto allShadersSatisfiedBy_v() {
      std::vector<uint64_t> ret;
      for(const auto& S : OD.CSRS)
	if(satisfies(resources, S.sourceProvidedResources) || satisfies(resources, S.targetProvidedResources))
	  ret.push_back(S.id);
      for(const auto& RP : OD.RPRS)
	for(const auto& S : RP.shaders)
	  if(satisfies(resources, S.sourceProvidedResources) || satisfies(resources, S.targetProvidedResources))
	    ret.push_back(S.id);
      return ret;
    };

    template<literalList<resourceReference> resources>
    static consteval size_t allShadersSatisfiedBy_c() {
      return allShadersSatisfiedBy_v<resources>().size();
    };

    template<literalList<resourceReference> resources>
    static consteval copyableArray<uint64_t, allShadersSatisfiedBy_c<resources>()> allShadersSatisfiedBy() {
      return allShadersSatisfiedBy_v<resources>();
    };

    template<size_t CNT> struct descriptorUpdateData_t {
      vk::WriteDescriptorSet writes[CNT];
      vk::DescriptorImageInfo images[CNT];//has gaps, index matches descriptor index, holds image view for future cleanup
      vk::DescriptorBufferInfo buffers[CNT];
      uint32_t writeCount;//a shader may use a subset of target data
      uint32_t skipCount;//util for filling this struct
      vk::DescriptorSet descriptorSet;
      uint64_t frameLastUpdated;
      void reset() {
	writeCount = skipCount = 0;
      };
    };

    //only shaders have descriptors
    template<literalList<uint64_t> shaders, bool forSource> struct descriptorTuple {
      static constexpr auto S = findShader<shaders[0]>();
      static constexpr size_t descriptorCount = calculateDescriptorCount(forSource ? S.sourceProvidedResources : S.targetProvidedResources);
      descriptorUpdateData_t<descriptorCount> data;
      descriptorTuple<shaders.sub(1), forSource> rest;
      template<size_t shaderId> auto& get() {
	PROFILEME;
	if constexpr(shaderId == S.id) {
	  return data;
	} else {
	  return rest.template get<shaderId>();
	}
      };
    };

    template<literalList<uint64_t> shaders, bool forSource> requires(shaders.len == 0)
      struct descriptorTuple<shaders, forSource> : std::false_type {};

    template<uint64_t> struct object_t;

    template<uint64_t TARGET_ID> class target_t {
    public:
      static_assert(containsId(OD.TLS, TARGET_ID));
      static constexpr size_t TARGET_IDX = findId(OD.TLS, TARGET_ID);
      static constexpr targetLayout TL = OD.TLS[TARGET_IDX];
      static_assert(containsId(OD.OLS, TL.objectLayoutId), "target layout reference missing object layout");
      static_assert(TL.resources.len > 0, "layouts must include at least one resource");
      static constexpr size_t maxFrameswap = frameswapLCM(TL.resources);
      static constexpr auto shaders = allShadersSatisfiedBy<TL.resources>();
      typedef descriptorTuple<shaders, false> perShaderByIdByFrame_t;

    private:
      onion<OD>* o;
      object_t<TL.objectLayoutId>* obj;
      std::map<uint64_t, frameBufferBundle[maxFrameswap]> fbByRpIdByFrame;
      perShaderByIdByFrame_t perShaderByIdByFrame[maxFrameswap];
      uint64_t objectId;

      target_t(const target_t&) = delete;
      target_t() = delete;

      inline mappedResourceTuple<TL.objectLayoutId>* allObjectResources() {
	return &obj->resources;
      };

      template<uint64_t stepId> inline bool stepEnabled() {
	return obj->stepEnablement.template enabled<stepId>();
      };

      friend onion;

    public:
      explicit target_t(onion<OD>* o): o(o) {};

      template<uint64_t resourceSlotId> auto& get() {
	PROFILEME;
	static_assert(findResourceReferenceToSlot(TL.resources, resourceSlotId));//sanity check that this layout uses that slot
	return allObjectResources()->template at<resourceSlotId>();
      };

      template<uint64_t resourceSlotId> void write(const auto& t, size_t hostAccessOffset = 0) {
	PROFILEME;
	scopeLock lock(&o->mutex);
	get<resourceSlotId>().set(o->frame + hostAccessOffset, t);
      };

    };

    template<size_t IDX = 0> struct targetCollectionTuple {
      iterableRecyclingPool<target_t<OD.TLS[IDX].id>, onion<OD>*> data;
      targetCollectionTuple<IDX+1> rest;

      targetCollectionTuple(onion<OD>* o) : data(o), rest(o) {};

      template<size_t TID, uint64_t TIDX = findId(OD.TLS, TID)> auto& ofLayout() {
	PROFILEME;
	if constexpr(IDX == TIDX)
	  return data;
	else
	  return rest.template ofLayout<TID, TIDX>();
      };

    };

    template<size_t IDX> requires(IDX == OD.TLS.len) struct targetCollectionTuple<IDX> {
      targetCollectionTuple(onion<OD>*) {};
    };

    targetCollectionTuple<> allTargets { this };

    template<uint64_t targetId> target_t<targetId>* createTarget() {
      PROFILEME;
      return allTargets.template ofLayout<targetId>().allocate();
    };

    template<uint64_t targetId> void freeTarget(target_t<targetId>* d) {
      PROFILEME;
      allTargets.template ofLayout<targetId>().free(d);
    };

    template<uint64_t SOURCE_ID> class source_t {
    public:
      static_assert(containsId(OD.SLS, SOURCE_ID), "source layout id not present in onion data");
      static constexpr size_t SOURCE_IDX = findId(OD.SLS, SOURCE_ID);
      static constexpr sourceLayout SL = OD.SLS[SOURCE_IDX];
      static_assert(SL.resources.len > 0, "layouts must include at least one resource");
      static_assert(containsId(OD.OLS, SL.objectLayoutId), "source layout reference missing object layout");
      static constexpr size_t maxFrameswap = frameswapLCM(SL.resources);
      static constexpr auto allObjectResourceSlots = getResourceSlots<SL.objectLayoutId>();
      static constexpr auto shaders = allShadersSatisfiedBy<SL.resources>();

    private:
      onion<OD>* o;
      object_t<SL.objectLayoutId>* obj;
      descriptorTuple<shaders, true> perShaderByIdByFrame[maxFrameswap];
      uint64_t objectId;

      source_t(const source_t&) = delete;
      source_t() = delete;

      inline mappedResourceTuple<SL.objectLayoutId>* allObjectResources() {
	return &obj->resources;
      };

      template<uint64_t stepId> inline bool stepEnabled() {
	return obj->stepEnablement.template enabled<stepId>();
      };

      friend onion;

    public:
      explicit source_t(onion<OD>* o): o(o) {};

      template<uint64_t resourceSlotId> auto& get() {
	PROFILEME;
	static_assert(findResourceReferenceToSlot(SL.resources, resourceSlotId));//sanity check that this layout uses that slot
	return allObjectResources()->template at<resourceSlotId>();
      };

      template<uint64_t resourceSlotId> void write(const auto& t, size_t hostAccessOffset = 0) {
	PROFILEME;
	scopeLock lock(&o->mutex);
	get<resourceSlotId>().set(o->frame + hostAccessOffset, t);
      };

    };

    template<size_t IDX = 0> struct sourceCollectionTuple {
      iterableRecyclingPool<source_t<OD.SLS[IDX].id>, onion<OD>*> data;
      sourceCollectionTuple<IDX+1> rest;

      sourceCollectionTuple(onion<OD>* o) : data(o), rest(o) {};

      template<size_t TID, uint64_t TIDX = findId(OD.SLS, TID)> auto& ofLayout() {
	PROFILEME;
	if constexpr(IDX == TIDX)
	  return data;
	else
	  return rest.template ofLayout<TID, TIDX>();
      };

    };

    template<size_t IDX> requires(IDX == OD.SLS.len) struct sourceCollectionTuple<IDX> {

      sourceCollectionTuple(onion<OD>*) {};

    };

    sourceCollectionTuple<> allSources { this };

    template<uint64_t sourceId> source_t<sourceId>* createSource() {
      PROFILEME;
      return allSources.template ofLayout<sourceId>().allocate();
    };

    template<uint64_t sourceId> void freeSource(source_t<sourceId>* d) {
      PROFILEME;
      allSources.template ofLayout<sourceId>().free(d);
    };

    template<literalList<sourceLayout> SLS> struct sourcePointerTuple {
      static constexpr uint64_t SID = SLS[0].id;
      typedef source_t<SID> T;
      T* data;
      sourcePointerTuple<SLS.sub(1)> rest;

      void createAll(onion<OD>* o, object_t<SLS[0].objectLayoutId>* obj, uint64_t objectId) {
	PROFILEME;
	data = o->createSource<SID>();
	data->obj = obj;
	data->objectId = objectId;
	if constexpr(SLS.len > 1)
	  rest.createAll(o, obj, objectId);
      };

      void freeAll(onion<OD>* o) {
	PROFILEME;
	o->freeSource<SID>(data);
	if constexpr(SLS.len > 1)
	  rest.freeAll(o);
      };

      template<uint64_t ID> auto* get() {
	PROFILEME;
	if constexpr(SID == ID)
	  return data;
	else
	  return rest.template get<ID>();
      };

    };

    template<literalList<sourceLayout> SLS> requires(SLS.len == 0) struct sourcePointerTuple<SLS> : std::false_type {};

    template<literalList<targetLayout> TLS> struct targetPointerTuple {
      static constexpr uint64_t TID = TLS[0].id;
      typedef target_t<TID> T;
      T* data;
      targetPointerTuple<TLS.sub(1)> rest;

      void createAll(onion<OD>* o, object_t<TLS[0].objectLayoutId>* obj, uint64_t objectId) {
	PROFILEME;
	data = o->createTarget<TID>();
	data->obj = obj;
	data->objectId = objectId;
	if constexpr(TLS.len > 1)
	  rest.createAll(o, obj, objectId);
      };

      void freeAll(onion<OD>* o) {
	PROFILEME;
	o->freeTarget<TID>(data);
	if constexpr(TLS.len > 1)
	  rest.freeAll(o);
      };

      template<uint64_t ID> auto* get() {
	PROFILEME;
	if constexpr(TID == ID)
	  return data;
	else
	  return rest.template get<ID>();
      };

    };

    template<literalList<targetLayout> SLS> requires(SLS.len == 0) struct targetPointerTuple<SLS> : std::false_type {};

    template<literalList<uint64_t> IDS> struct alignas(1) stepEnablementTuple {
      static constexpr uint64_t ID = IDS[0];
      bool isEnabled = true;
      stepEnablementTuple<IDS.sub(1)> rest;
      template<uint64_t id> bool& enabled() {
	if constexpr(id == ID)
	  return isEnabled;
	else
	  return rest.template enabled<id>();
      };
    };

    template<literalList<uint64_t> IDS> requires(IDS.len == 0) struct stepEnablementTuple<IDS> : public std::false_type {
    };

    static consteval std::vector<uint64_t> getStepIds_v(literalList<resourceReference> rrs) {
      std::vector<uint64_t> consumers;
      for(auto& rr : rrs)
	consumers.push_back(rr.resourceConsumerId);
      std::vector<uint64_t> ret;
      for(size_t layerIdx = 0;layerIdx < OD.LRS.len;layerIdx++) {
	const layerRequirements& layer = OD.LRS[layerIdx];
	//copy:
	for(uint64_t substepId : layer.copies) {
	  const auto& substep = findById(OD.CSS, substepId);
	  if(contains(consumers, substep.src) || contains(consumers, substep.dst))
	    ret.push_back(substepId);
	}
	//clear:
	for(uint64_t substepId : layer.clears) {
	  const auto& substep = findById(OD.CLS, substepId);
	  if(contains(consumers, substep.id))
	    ret.push_back(substepId);
	}
	//rendering:
	for(uint64_t passIdx = 0;passIdx < layer.renders.len;passIdx++) {
	  uint64_t passId = layer.renders[passIdx];
	  auto pass = findById(OD.RPRS, passId);
	  auto depthRR = findResourceReferenceToConsumer(rrs, pass.depth);
	  bool inPass = satisfies(rrs, pass.color) &&
	    satisfies(rrs, pass.input) &&
	    (depthRR != NULL || pass.depth == NONE);
	  for(const graphicsShaderRequirements& gsr : pass.shaders) {
	    //NOTE: graphical shaders use shader id of 0 because all shaders in a render pass are considered concurrent.
	    for(const resourceConsumer& srr : gsr.targetProvidedResources)
	      if(contains(consumers, srr.id))
		inPass = true;
	    for(const resourceConsumer& srr : gsr.sourceProvidedResources)
	      if(contains(consumers, srr.id))
		inPass = true;
	    if(inPass)
	      ret.push_back(gsr.id);
	  }
	  if(inPass)
	    ret.push_back(passId);
	}
	//compute:
	for(uint64_t csrId : layer.computeShaders) {
	  const computeShaderRequirements& csr = findById(OD.CSRS, csrId);
	  for(const resourceConsumer& srr : csr.targetProvidedResources)
	    if(contains(consumers, srr.id))
	      ret.push_back(csrId);
	  for(const resourceConsumer& srr : csr.sourceProvidedResources)
	    if(contains(consumers, srr.id))
	      ret.push_back(csrId);
	}
      }
      std::sort(ret.begin(), ret.end());
      ret.resize(std::unique(ret.begin(), ret.end()) - ret.begin());
      return ret;
    };

    static consteval size_t getStepIds_c(literalList<resourceReference> rrs) {
      return getStepIds_v(rrs).size();
    };

    template<literalList<resourceReference> rrs> static consteval copyableArray<uint64_t, getStepIds_c(rrs)> getStepIds() {
      return getStepIds_v(rrs);
    };

    template<uint64_t objectLayoutId>
    struct object_t {
      static_assert(containsId(OD.OLS, objectLayoutId), "object layout id not present in onion data");
      static constexpr auto RSS = getResourceSlots<objectLayoutId>();
      static_assert(RSS.len > 0, "objects must include at least one resource");
      static constexpr auto SLS = getSourceLayouts<objectLayoutId>();
      static constexpr auto TLS = getTargetLayouts<objectLayoutId>();
      static constexpr auto allRRS = getResourceReferences<objectLayoutId>();
      static constexpr objectLayout OL = findById(OD.OLS, objectLayoutId);
      static constexpr bool hasWindow = OL.windowConsumerId != NONE;
      static constexpr auto allStepIds = getStepIds<allRRS>();

      uint64_t objectId;
      mappedResourceTuple<objectLayoutId> resources;
      targetPointerTuple<TLS> targets;
      sourcePointerTuple<SLS> sources;
      stepEnablementTuple<allStepIds> stepEnablement;
      std::conditional_t<hasWindow, window, size_t> presentWindow = { OD.GPUID };
      onion<OD>* owner;
      uint64_t frameCreated;

      explicit object_t(onion<OD>* o) : owner(o) {};

      inline auto& getWindow() {
	PROFILEME;
	return presentWindow;
      };

      template<uint64_t resourceSlotId> auto& get() {
	static_assert_show(containsId<resourceSlot>(RSS, resourceSlotId), resourceSlotId);
	PROFILEME;
	return resources.template at<resourceSlotId>();
      };

      template<uint64_t resourceSlotId> void write(const auto& t, size_t hostAccessOffset = 0) {
	static_assert_show(containsId<resourceSlot>(RSS, resourceSlotId), resourceSlotId);
	PROFILEME;
	scopeLock lock(&owner->mutex);
	get<resourceSlotId>().set(owner->frame + hostAccessOffset, t);
      };

      template<uint64_t resourceSlotId> void set(auto* t) {
	PROFILEME;
	scopeLock lock(&owner->mutex);
	resources.template set<resourceSlotId>(t, owner->frame);
      };

      template<uint64_t id> inline bool& stepEnabled() {
	return stepEnablement.template enabled<id>();
      };

      inline void preRender(vk::CommandBuffer cmd) {
	PROFILEME;
	if constexpr(hasWindow) {
	  auto windowExt = presentWindow.getSize3D();
	  resources.trackWindowSize(owner->frame, windowExt);
	  // presentWindow.acquire();
	}
	resources.preRender(owner->frame, cmd, owner->getActiveGarbageCollector());
      };

      inline void postRender(vk::SemaphoreSubmitInfo& renderWaitSem) {
	PROFILEME;
	if constexpr(hasWindow) {
	  static constexpr resourceReference windowRR = *findResourceReferenceToConsumer(allRRS, findById(OD.OLS, objectLayoutId).windowConsumerId);
	  auto& img = get<windowRR.resourceSlotId>();
	  static constexpr vk::ImageLayout layout = getLayoutForImage(windowRR.resourceSlotId, { windowRR.frameLatency, layerIdx_OPTE, substep_e::post }).layout;
	  static_assert(layout == vk::ImageLayout::eGeneral || layout == vk::ImageLayout::eTransferSrcOptimal);
	  auto f = owner->frame + windowRR.frameLatency;
	  presentWindow.present(img.frameImage(f), layout, img.getSizeOffset(f), renderWaitSem);
	}
      };

      void reinit(uint64_t frame, vk::CommandBuffer cmd) {
	PROFILEME;
	resources.init(frame, cmd);
	objectId = owner->od.objectIdSeed++;
	if constexpr(TLS.len)
	  targets.createAll(owner, this, objectId);
	if constexpr(SLS.len)
	  sources.createAll(owner, this, objectId);
	if constexpr(hasWindow)
	  presentWindow.show();
      };

      void free() {
	PROFILEME;
	if constexpr(hasWindow)
	  presentWindow.hide();
	if constexpr(TLS.len)
	  targets.freeAll(owner);
	if constexpr(SLS.len)
	  sources.freeAll(owner);
      };

    };

    template<size_t IDX = 0> struct objectCollectionTuple {
      iterableRecyclingPool<object_t<OD.OLS[IDX].id>, onion<OD>*> data;
      objectCollectionTuple<IDX+1> rest;

      objectCollectionTuple(onion<OD>* o) : data(o), rest(o) {};

      template<size_t OID, uint64_t OIDX = findId(OD.OLS, OID)> auto& ofLayout() {
	PROFILEME;
	if constexpr(IDX == OIDX)
	  return data;
	else
	  return rest.template ofLayout<OID, OIDX>();
      };

    };

    template<size_t IDX> requires(IDX == OD.OLS.len) struct objectCollectionTuple<IDX> {

      objectCollectionTuple(onion<OD>*) {};

    };

    objectCollectionTuple<> allObjects { this };

    template<uint64_t objectLayoutId> object_t<objectLayoutId>* create() {
      PROFILEME;
      object_t<objectLayoutId>* ret;
      {
	scopeLock lock(&mutex);//TODO eliminate this lock by stashing created objects into a pending container (per object, with its own mutex) that gets processed between renders
	ret = allObjects.template ofLayout<objectLayoutId>().allocate();
	ret->frameCreated = frame;
      }
      auto cmd = dev->getTempCmd();
      ret->reinit(frame, cmd.cmd);
      cmd.submit();
      cmd.waitFor();
      return ret;
    };

    template<uint64_t objectLayoutId> void destroy(object_t<objectLayoutId>* d) {
      PROFILEME;
      scopeLock lock(&mutex);
      d->free();
      allObjects.template ofLayout<objectLayoutId>().free(d);
    };

    template<literalList<resourceSlot> RSS,
	     literalList<resourceReference> RRS,
	     literalList<resourceConsumer> RCS,
	     resourceBarrierTiming RBT,
	     size_t descriptors, uint64_t objectId,
	     uint64_t RCIDX = 0>
    inline void fillWrites(descriptorUpdateData_t<descriptors>& data, mappedResourceTuple<objectId>& rm, vk::DescriptorSet ds,
			   uint64_t frameMod, uint64_t frameLastUpdated, uint64_t* waitFrameOut) {
      if constexpr(RCIDX < RCS.len) {
	{
	  PROFILEME;
	  static constexpr resourceConsumer RC = RCS[RCIDX];
	  static constexpr resourceReference RR = *findResourceReferenceToConsumer(RRS, RC.id);
	  static constexpr resourceSlot RS = findById(RSS, RR.resourceSlotId);
	  if constexpr(RC.usage.type == resourceUsageType::eDescriptor) {
	    auto& res = rm.template at<RS.id>();
	    if(frameLastUpdated == NONE ||
	       frameLastUpdated < rm.template frameLastUpdatedExternal<RS.id>() ||
	       frameLastUpdated < res.frameUpdated(frameMod)) {
	      auto& w = data.writes[data.writeCount];
	      w.dstSet = ds;
	      w.dstBinding = data.writeCount + data.skipCount;
	      w.dstArrayElement = 0;
	      w.descriptorCount = 1;
	      w.descriptorType = RC.usage.asDescriptor.descriptorType;
	      if constexpr(containsId(OD.IRS, RS.requirementId)) {
		w.pBufferInfo = NULL;
		auto& img = data.images[w.dstBinding];
		w.pImageInfo = &img;
		if constexpr(RC.usage.asDescriptor.descriptorType == vk::DescriptorType::eCombinedImageSampler ||
			     RC.usage.asDescriptor.descriptorType == vk::DescriptorType::eSampler) {
		  img.sampler = dev->getSampler<RC.usage.asDescriptor.sampler>();
		}
		if(img.imageView)
		  getActiveGarbageCollector().push(img.imageView);
		img.imageView = res.template createView<RR.subresource.viewType,
							getSubresource(RS.requirementId, RR)>(frameMod);
		img.imageLayout = getLayoutForImage(RR.resourceSlotId, { RR.frameLatency, RBT }).layout;
		if(waitFrameOut) [[likely]]
		  *waitFrameOut = max(*waitFrameOut, frame - findById(OD.IRS, RS.requirementId).frameswapCount);
	      } else {
		w.pImageInfo = NULL;
		auto& buf = data.buffers[w.dstBinding];
		w.pBufferInfo = &buf;
		buf.buffer = res.frameBuffer(frameMod);
		buf.offset = RR.subresource.offset;
		buf.range = RR.subresource.length ? RR.subresource.length : VK_WHOLE_SIZE;
		if(waitFrameOut) [[likely]]
		  *waitFrameOut = max(*waitFrameOut, frame - findById(OD.BRS, RS.requirementId).frameswapCount);
		// WARN("wrote buffer descriptor ", buf.buffer, " to binding ", w.dstBinding, " on set ", ds);
	      }
	      data.writeCount++;
	    } else {
	      data.skipCount++;
	    }
	  }
	}
	fillWrites<RSS, RRS, RCS, RBT, descriptors, objectId, RCIDX+1>(data, rm, ds, frameMod, frameLastUpdated, waitFrameOut);
      }
    };

    template<literalList<resourceSlot> RSS,
	     literalList<resourceReference> RRS,
	     literalList<resourceConsumer> RCS,
	     resourceBarrierTiming RBT,
	     size_t descriptors, uint64_t objectId>
    inline void prepareDescriptors(descriptorUpdateData_t<descriptors>& descriptorBundle,
				   std::unique_ptr<descriptorPoolPoolBase>& dpp,
				   mappedResourceTuple<objectId>& resources,
				   size_t frameMod) {
      PROFILEME;
      if(!descriptorBundle.descriptorSet) [[unlikely]] {
	if(!dpp) [[unlikely]]
	  dpp.reset(new descriptorPoolPool<RCS, OD.GPUID>());
	descriptorBundle.descriptorSet = dpp->allocate();
	descriptorBundle.reset();
	fillWrites<RSS, RRS, RCS, RBT, descriptors>(descriptorBundle, resources, descriptorBundle.descriptorSet, frameMod, NONE, NULL);
	dev->getVkDevice().updateDescriptorSets(descriptorBundle.writeCount, descriptorBundle.writes, 0, NULL);
	descriptorBundle.frameLastUpdated = frame;
      } else if(descriptorBundle.frameLastUpdated < resources.frameLastUpdated(frame)) [[unlikely]] {
	descriptorBundle.reset();
	uint64_t waitFrame = 0;
	fillWrites<RSS, RRS, RCS, RBT>(descriptorBundle, resources, descriptorBundle.descriptorSet, frameMod, descriptorBundle.frameLastUpdated, &waitFrame);
	waitForFrame(waitFrame);
	dev->getVkDevice().updateDescriptorSets(descriptorBundle.writeCount, descriptorBundle.writes, 0, NULL);
	descriptorBundle.frameLastUpdated = frame;
      }
    };

    static WITE_DEBUG_IB_CE std::vector<resourceBarrier> allBarriers() {
      std::vector<resourceBarrier> allBarriers;
      for(const resourceSlot& RS : OD.RSS)
	concat(allBarriers, findBarriers(RS.id));
      return allBarriers;
    };

    static consteval std::vector<resourceBarrier> barriersForTime_v(resourceBarrierTiming BT) {
      auto ret = allBarriers();
      std::erase_if(ret, [BT](auto x){ return x.timing.barrierId != BT; });
      return ret;
    };

    static consteval size_t barriersForTime_cnt(resourceBarrierTiming BT) {
      auto vector = barriersForTime_v(BT);
      size_t ret = vector.size();
      vector.clear();
      return ret;
    };

    template<resourceBarrierTiming BT>
    static consteval copyableArray<resourceBarrier, barriersForTime_cnt(BT)> barriersForTime() {
      return barriersForTime_v(BT);
    };

    template<uint64_t layoutId> inline auto& getAllOfLayout() {
      PROFILEME;
      if constexpr(containsId(OD.SLS, layoutId))
	return allSources.template ofLayout<layoutId>();
      else
	return allTargets.template ofLayout<layoutId>();
    };

    template<uint64_t layoutId> static consteval inline auto findLayoutById() {
      if constexpr(containsId(OD.SLS, layoutId))
	return findById(OD.SLS, layoutId);
      else if constexpr(containsId(OD.TLS, layoutId))
	return findById(OD.TLS, layoutId);
    };

    template<literalList<resourceBarrier> RBS, resourceBarrierTiming BT> inline void recordBarriers(vk::CommandBuffer cmd) {
      if constexpr(RBS.len) {
	{
	  PROFILEME;
	  constexpr size_t barrierBatchSize = 256;
	  constexpr resourceBarrier RB = RBS[0];
	  constexpr bool isImage = containsId(OD.IRS, RB.requirementId);
	  vk::DependencyInfo DI;
	  if constexpr(isImage) {
	    //NOTE: This function is under the onion mutex.
	    //the only field that ever changes is the image handle itself, so save this for future reuse.
	    static copyableArray<vk::ImageMemoryBarrier2, barrierBatchSize> barriers(vk::ImageMemoryBarrier2 {
		toPipelineStage2(RB.before),
		RB.before.access,
		toPipelineStage2(RB.after),
		RB.after.access,
		RB.beforeLayout.layout,
		RB.afterLayout.layout,
		{}, {}, {},
		getAllInclusiveSubresource(findById(OD.IRS, RB.requirementId))
	      });
	    DI.pImageMemoryBarriers = barriers.ptr();
	    DI.imageMemoryBarrierCount = barrierBatchSize;
	    size_t mbIdx = 0;
	    for(object_t<RB.objectLayoutId>* cluster : allObjects.template ofLayout<RB.objectLayoutId>()) {
	      if(cluster->frameCreated >= frame) [[unlikely]] continue;
	      auto& img = cluster->template get<RB.resourceSlotId>();
	      barriers[mbIdx].image = img.frameImage(RB.timing.frameLatency + frame);
	      WITE_DEBUG_IBT(barriers[mbIdx], cmd, BT);
	      mbIdx++;
	      if(mbIdx == barrierBatchSize) [[unlikely]] {
		cmd.pipelineBarrier2(&DI);
		mbIdx = 0;
	      }
	    }
	    if(mbIdx) [[likely]] {
	      DI.imageMemoryBarrierCount = mbIdx;
	      cmd.pipelineBarrier2(&DI);
	    }
	  } else {
	    static copyableArray<vk::BufferMemoryBarrier2, barrierBatchSize> barriers(vk::BufferMemoryBarrier2 {
		toPipelineStage2(RB.before),
		RB.before.access,
		toPipelineStage2(RB.after),
		RB.after.access,
		{}, {}, {}, 0, VK_WHOLE_SIZE
	      });
	    DI.pBufferMemoryBarriers = barriers.ptr();
	    DI.bufferMemoryBarrierCount = barrierBatchSize;
	    size_t mbIdx = 0;
	    for(object_t<RB.objectLayoutId>* cluster : allObjects.template ofLayout<RB.objectLayoutId>()) {
	      if(cluster->frameCreated >= frame) [[unlikely]] continue;
	      barriers[mbIdx].buffer = cluster->template get<RB.resourceSlotId>().frameBuffer(RB.timing.frameLatency + frame);
	      mbIdx++;
	      if(mbIdx == barrierBatchSize) [[unlikely]] {
		cmd.pipelineBarrier2(&DI);
		mbIdx = 0;
	      }
	    }
	    if(mbIdx) [[likely]] {
	      DI.bufferMemoryBarrierCount = mbIdx;
	      cmd.pipelineBarrier2(&DI);
	    }
	  }
	}
	recordBarriers<RBS.sub(1), BT>(cmd);
      }
    };

    template<resourceBarrierTiming BT> inline void recordBarriersForTime(vk::CommandBuffer cmd) {
      PROFILEME;
#ifdef WITE_DEBUG_IMAGE_BARRIERS
      WARN("recording barriers for timing: layerIdx: ", BT.layerIdx, ", substep: ", (int)BT.substep, ", pass/shader idx: ", BT.passOrShaderIdx, " on frame: ", frame);
#endif
      static constexpr auto BTS = barriersForTime<BT>();
      recordBarriers<BTS, BT>(cmd);
    };

    template<size_t layerIdx, copyStep CS, literalList<uint64_t> XLS> inline void recordCopies_l2(vk::CommandBuffer cmd) {
      if constexpr(XLS.len) {
	{
	  PROFILEME;
	  static constexpr auto XL = findLayoutById<XLS[0]>();//could be targetLayout or sourceLayout
	  static constexpr const resourceReference *SRR = findResourceReferenceToConsumer(XL.resources, CS.src),
	    *DRR = findResourceReferenceToConsumer(XL.resources, CS.dst);
	  if constexpr(SRR && DRR) {//skip copy if they're not both represented
	    static constexpr resourceSlot SRS = findById(OD.RSS, SRR->resourceSlotId),
	      DRS = findById(OD.RSS, DRR->resourceSlotId);
	    static constexpr bool srcIsImage = containsId(OD.IRS, SRS.requirementId),
	      dstIsImage = containsId(OD.IRS, DRS.requirementId);
	    if constexpr(srcIsImage && dstIsImage) {
	      //always blit, no way to know at compile time if they're the same size
	      //TODO resolve if different sample counts
	      vk::ImageBlit blitInfo(getAllInclusiveSubresourceLayers(findById(OD.IRS, SRS.requirementId)), {},
				     getAllInclusiveSubresourceLayers(findById(OD.IRS, DRS.requirementId)), {});
	      for(auto* cluster : getAllOfLayout<XL.id>()) {
		if(cluster->obj->frameCreated >= frame) [[unlikely]] continue;
		if(!cluster->template stepEnabled<CS.id>()) [[unlikely]] continue;
		//TODO if same size (and mip?), use ImageCopy
		auto& src = cluster->template get<SRS.id>();
		auto& dst = cluster->template get<DRS.id>();
		blitInfo.srcOffsets[1] = src.getSizeOffset(SRR->frameLatency + frame);
		blitInfo.dstOffsets[1] = dst.getSizeOffset(DRR->frameLatency + frame);
		cmd.blitImage(src.frameImage(SRR->frameLatency + frame),
			      getLayoutForImage(SRS.id, { SRR->frameLatency, layerIdx, substep_e::copy }).layout,
			      dst.frameImage(DRR->frameLatency + frame),
			      getLayoutForImage(DRS.id, { DRR->frameLatency, layerIdx, substep_e::copy }).layout,
			      1, &blitInfo, CS.filter);
	      }
	    } else if constexpr(!srcIsImage && dstIsImage) {
	      //some image formats cannot be staged as images on some devices/drivers
	      vk::BufferImageCopy copyInfo(SRR->subresource.offset, 0, 0,
					   //w/h set to 0 means calculate assuming "tightly packed"
					   getAllInclusiveSubresourceLayers(findById(OD.IRS, DRS.requirementId)), {}, {});
	      for(auto* cluster : getAllOfLayout<XL.id>()) {
		if(cluster->obj->frameCreated >= frame) [[unlikely]] continue;
		if(!cluster->template stepEnabled<CS.id>()) [[unlikely]] continue;
		auto& src = cluster->template get<SRS.id>();
		auto& dst = cluster->template get<DRS.id>();
		copyInfo.imageExtent = dst.getSizeExtent(DRR->frameLatency + frame);
		//this uses the image to decide how much buffer to copy, so check the buffer is at least that big
		ASSERT_TRAP(copyInfo.imageExtent.width * copyInfo.imageExtent.height * copyInfo.imageExtent.depth * sizeofFormat<findById(OD.IRS, DRS.requirementId).format>() <= findById(OD.BRS, SRS.requirementId).size, "image (", DRS.requirementId, ") too big to populate from buffer (", SRS.requirementId, ")");
		cmd.copyBufferToImage(src.frameBuffer(SRR->frameLatency + frame),
				      dst.frameImage(DRR->frameLatency + frame),
				      getLayoutForImage(DRS.id, { DRR->frameLatency, layerIdx, substep_e::copy }).layout,
				      1, &copyInfo);
	      }
	    } else if constexpr(srcIsImage && !dstIsImage) {
	      static_assert(false, "NYI image to buffer copy");
	    } else {//buffer to buffer
	      static constexpr bufferRequirements SBR = findById(OD.BRS, SRS.requirementId),
		TBR = findById(OD.BRS, DRS.requirementId);
	      static constexpr vk::BufferCopy copyInfo(SRR->subresource.offset, DRR->subresource.offset, min(SRR->subresource.length != VK_WHOLE_SIZE ? SRR->subresource.length : (SBR.size - SRR->subresource.offset), DRR->subresource.length != VK_WHOLE_SIZE ? DRR->subresource.length : (TBR.size - DRR->subresource.offset)));
	      for(auto* cluster : getAllOfLayout<XL.id>()) {
		if(cluster->obj->frameCreated >= frame) [[unlikely]] continue;
		if(!cluster->template stepEnabled<CS.id>()) [[unlikely]] continue;
		auto& src = cluster->template get<SRS.id>();
		auto& dst = cluster->template get<DRS.id>();
		cmd.copyBuffer(src.frameBuffer(SRR->frameLatency + frame),
			       dst.frameBuffer(DRR->frameLatency + frame),
			       1, &copyInfo);
		// WARN("copied buffer ", src.frameBuffer(CS.src.frameLatency + frame), " to ", dst.frameBuffer(CS.dst.frameLatency + frame));
	      }
	    }
	  }
	}
	recordCopies_l2<layerIdx, CS, XLS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx, literalList<uint64_t> CSIDS> inline void recordCopies(vk::CommandBuffer cmd) {
      if constexpr(CSIDS.len) {
	static constexpr copyStep CS = findById(OD.CSS, CSIDS[0]);
	recordCopies_l2<layerIdx, CS, allLayoutIds>(cmd);
	recordCopies<layerIdx, CSIDS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx, clearStep CS, literalList<uint64_t> XLS> inline void recordClears_l2(vk::CommandBuffer cmd) {
      if constexpr(XLS.len) {
	{
	  PROFILEME;
	  static constexpr auto XL = findLayoutById<XLS[0]>();//source or target layout
	  static constexpr const resourceReference* RR = findResourceReferenceToConsumer(XL.resources, CS.id);
	  if constexpr(RR) {
	    static constexpr resourceSlot RS = findById(OD.RSS, RR->resourceSlotId);
	    static_assert(containsId(OD.IRS, RS.requirementId));
	    static constexpr imageRequirements IR = findById(OD.IRS, RS.requirementId);
	    static constexpr vk::ImageSubresourceRange SR = getAllInclusiveSubresource(IR);
	    for(auto& cluster : getAllOfLayout<XL.id>()) {
	      if(cluster.obj->frameCreated >= frame) [[unlikely]] continue;
	      if(!cluster.template stepEnabled<CS.id>()) [[unlikely]] continue;
	      auto img = cluster->template get<RS.id>().frameImage(RR->frameLatency + frame);
	      if constexpr(isDepth(IR)) {
		cmd.clearDepthStencilImage(img, getLayoutForImage(RS.id, { RR->frameLatency, layerIdx, substep_e::clear }),
					   &CS.clearValue.depthStencil, 1, &SR);
	      } else {
		cmd.clearColorImage(img, getLayoutForImage(RS.id, { RR->frameLatency, layerIdx, substep_e::clear }),
				    &CS.clearValue.color, 1, &SR);
	      }
	    }
	  }
	}
	recordClears_l2<CS, XLS.sub(1)>(cmd);
      }
    };

    template<literalList<uint64_t> CLIDS> inline void recordClears(vk::CommandBuffer cmd) {
      if constexpr(CLIDS.len) {
	static constexpr clearStep CS = findById(OD.CLS, CLIDS[0]);
	recordClears_l2<CS, allLayoutIds>(cmd);
	recordClears<CLIDS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx, size_t rpIdx, resourceConsumer RC, const resourceReference* RR, bool clear, uint32_t idx> struct attachmentInfo {
      static constexpr resourceSlot RS = findById(OD.RSS, RR->resourceSlotId);
      static constexpr imageRequirements IR = findById(OD.IRS, RS.requirementId);
      static constexpr vk::ImageLayout layout = getLayoutForImage(RS.id, { RR->frameLatency, layerIdx, substep_e::render, rpIdx }).layout;
      static constexpr vk::AttachmentReference AR { idx, layout };
      static constexpr vk::AttachmentDescription AD { {}, IR.format, vk::SampleCountFlagBits::e1, clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, layout, AR.layout };
      static constexpr uint32_t len = 1;
    };

    template<size_t layerIdx, size_t rpIdx, resourceConsumer RC, const resourceReference* RR, bool clear, uint32_t idx> requires(RC.id == NONE || RR == NULL)
      struct attachmentInfo<layerIdx, rpIdx, RC, RR, clear, idx> {
	//for the depth member of depth-less RPs
	static constexpr vk::AttachmentReference AR { };
	static constexpr vk::AttachmentDescription AD { };
	static constexpr uint32_t len = 0;
      };

    template<size_t layerIdx, size_t rpIdx, literalList<uint64_t> RC_IDL, targetLayout TL, bool clear, uint32_t idx, vk::AccessFlagBits2 A> struct attachmentInfoTuple {
      static constexpr uint64_t ID = RC_IDL[0];
      typedef attachmentInfo<layerIdx, rpIdx, resourceConsumer(ID, vk::ShaderStageFlagBits::eFragment, A), findResourceReferenceToConsumer(TL.resources, ID), clear, idx> AI;
      typedef attachmentInfoTuple<layerIdx, rpIdx, RC_IDL.sub(1), TL, clear, idx+1, A> rest;
      static constexpr auto AR_all = concat<vk::AttachmentReference, AI::AR, rest::AR_all>();
      static constexpr auto AD_all = concat<vk::AttachmentDescription, AI::AD, rest::AD_all>();
    };

    template<size_t layerIdx, size_t rpIdx, literalList<uint64_t> RC_IDL, targetLayout TL, bool clear, uint32_t idx, vk::AccessFlagBits2 A> requires(!RC_IDL)
    struct attachmentInfoTuple<layerIdx, rpIdx, RC_IDL, TL, clear, idx, A> {
      static constexpr copyableArray<vk::AttachmentReference, 0> AR_all;
      static constexpr copyableArray<vk::AttachmentDescription, 0> AD_all;
    };

    template<size_t layerIdx, renderPassRequirements RP, targetLayout TL> struct renderPassInfoBundle {
      static constexpr size_t firstInput = RP.color.count(), firstDepth = RP.input.count() + RP.color.count();
      static constexpr size_t rpIdx = findIdx(OD.LRS[layerIdx].renders, RP.id);
      typedef attachmentInfoTuple<layerIdx, rpIdx, RP.input, TL, false, firstInput, vk::AccessFlagBits2::eInputAttachmentRead> inputAIS;
      typedef attachmentInfoTuple<layerIdx, rpIdx, RP.color, TL, RP.clearColor, 0, vk::AccessFlagBits2::eColorAttachmentRead> colorAIS;
      typedef attachmentInfo<layerIdx, rpIdx, consumerForDepthAttachment(RP.depth), findResourceReferenceToConsumer(TL.resources, RP.depth), RP.clearDepth, firstDepth> depthAI;
      static constexpr vk::SubpassDescription subpass { {}, vk::PipelineBindPoint::eGraphics, inputAIS::AR_all.len, inputAIS::AR_all.ptr(), colorAIS::AR_all.len, colorAIS::AR_all.ptr(), NULL, depthAI::len ? &depthAI::AR : NULL };
      static constexpr literalList<vk::AttachmentDescription> depthWrapper = depthAI::len ? literalList<vk::AttachmentDescription>{depthAI::AD} : literalList<vk::AttachmentDescription>{};
      static constexpr auto attachments = concat<vk::AttachmentDescription, colorAIS::AD_all, inputAIS::AD_all, depthWrapper>();
      //TODO multiview
      //static constexpr vk::RenderPassMultiviewCreateInfo multiview {
      static constexpr vk::RenderPassCreateInfo rpci { {}, attachments.len, attachments.ptr(), 1, &subpass, 0 };
    };

    //MAYBE match like clusters of shaders, sources, and targets so they can share layouts and descriptor pools?

    template<size_t layerIdx, targetLayout TL, renderPassRequirements RP, graphicsShaderRequirements GSR, literalList<sourceLayout> SLS>
    inline void recordRenders_l5(auto& target, perTargetLayout& ptl, perTargetLayoutPerShader& ptlps, descriptorUpdateData_t<GSR.targetProvidedResources.len>& targetDescriptors, vk::RenderPass rp, vk::CommandBuffer cmd) {
      if constexpr(SLS.len) {
	{
	  PROFILEME;
	  static constexpr sourceLayout SL = SLS[0];
	  static constexpr bool isMesh = containsStage<GSR.modules, vk::ShaderStageFlagBits::eMeshEXT>();
	  // WARN("  using source ", SL.id);
	  // WARN("  isMesh ", isMesh);
	  if constexpr(satisfies(SL.resources, GSR.sourceProvidedResources)) {
	    //for now (at least), only one vertex binding of each input rate type. Source only.
	    static_assert(GSR.sourceProvidedResources.countWhereCE(findVB<SL.resources>()) <= 1);
	    static_assert(GSR.sourceProvidedResources.countWhereCE(findIB<SL.resources>()) <= 1);
	    static_assert(GSR.sourceProvidedResources.countWhereCE(findIndex<SL.resources>()) <= 1);
	    static_assert(GSR.sourceProvidedResources.countWhereCE(findIndirect<SL.resources>()) <= 1);
	    static_assert(GSR.sourceProvidedResources.countWhereCE(findIndirectCount<SL.resources>()) <= 1);
	    static constexpr const resourceConsumer* vb = GSR.sourceProvidedResources.firstWhere(findVB<SL.resources>());
	    static constexpr const resourceConsumer* ib = GSR.sourceProvidedResources.firstWhere(findIB<SL.resources>());
	    static constexpr const resourceConsumer* indexConsumer = GSR.sourceProvidedResources.firstWhere(findIndex<SL.resources>());
	    static constexpr const resourceConsumer* indirectConsumer = GSR.sourceProvidedResources.firstWhere(findIndirect<SL.resources>());
	    static constexpr const resourceConsumer* countConsumer = GSR.sourceProvidedResources.firstWhere(findIndirectCount<SL.resources>());
	    static constexpr size_t vibCount = (ib ? 1 : 0) + (vb ? 1 : 0);
	    shaderInstance& shaderInstance = ptlps.perSL[SL.id];
	    perSourceLayoutPerShader& pslps = od.perSL[SL.id].perShader[GSR.id];
	    if(!pslps.descriptorPool) [[unlikely]]
	      pslps.descriptorPool.reset(new descriptorPoolPool<GSR.sourceProvidedResources, OD.GPUID>());
	    if(!shaderInstance.pipeline) [[unlikely]] {
	      PROFILEME_MSG("pipeline creation, nested");
	      if(shaderInstance.pipelineLayout == NULL) {
		static constexpr vk::DescriptorSetLayoutCreateInfo dslcis[] = {
		  descriptorPoolPool<GSR.sourceProvidedResources, OD.GPUID>::dslci,
		  descriptorPoolPool<GSR.targetProvidedResources, OD.GPUID>::dslci
		};
		shaderInstance.pipelineLayout = dev->getPipelineLayout<dslcis>();
	      }
	      vk::PipelineShaderStageCreateInfo stages[GSR.modules.len];
	      createModules<GSR.modules, OD.GPUID>(stages);
	      static constexpr copyableArray<resourceConsumer, vibCount> vib = [](size_t i){ return i && vb ? *ib : *vb; };
	      static constexpr auto vibs = getBindingDescriptions<vib>();
	      static constexpr auto viad = getAttributeDescriptions<vib>();
	      static constexpr vk::PipelineVertexInputStateCreateInfo verts { {}, vibCount, vibs.ptr(), viad.LENGTH, viad.ptr() };
	      static constexpr vk::PipelineInputAssemblyStateCreateInfo assembly { {}, GSR.topology, false };
	      //TODO tessellation
	      //viewport and scissors are dynamic so we don't need to rebuild the pipe when the target size changes
	      static constexpr vk::PipelineViewportStateCreateInfo vp = { {}, 1, NULL, 1, NULL };
	      static constexpr vk::PipelineRasterizationStateCreateInfo raster = { {}, false, false, vk::PolygonMode::eFill, GSR.cullMode, GSR.windCounterClockwise ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise, false, 0, 0, 0, 1.0f };
	      static constexpr vk::PipelineMultisampleStateCreateInfo multisample = { {}, vk::SampleCountFlagBits::e1, 0, 0, NULL, 0, 0 };
	      // static constexpr vk::StencilOpState stencilOp = {vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 0, 0 };
	      static constexpr vk::PipelineDepthStencilStateCreateInfo depth = { {}, RP.depth != NONE, GSR.depthWrite, GSR.depthCompareMode };//not set: depth bounds and stencil test stuff
	      static constexpr vk::PipelineColorBlendStateCreateInfo blend = { {}, false, vk::LogicOp::eNoOp, GSR.blend.len, GSR.blend.data, { 1, 1, 1, 1 } };
	      static_assert(GSR.blend.len == RP.color.len || RP.color.len == 0, "must match unless there are no colors");
	      static constexpr vk::DynamicState dynamics[] = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
	      static constexpr vk::PipelineDynamicStateCreateInfo dynamic = { {}, 2, dynamics };
	      vk::GraphicsPipelineCreateInfo gpci { {}, GSR.modules.len, stages, &verts, &assembly, /*tesselation*/ NULL, &vp, &raster, &multisample, &depth, &blend, &dynamic, shaderInstance.pipelineLayout, rp, /*subpass idx*/ 0 };//not set: derivitive pipeline stuff
	      VK_ASSERT(dev->getVkDevice().createGraphicsPipelines(dev->getPipelineCache(), 1, &gpci, ALLOCCB, &shaderInstance.pipeline), "Failed to create graphics pipeline ", GSR.id);
	    }
	    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderInstance.pipeline);
	    if constexpr(GSR.targetProvidedResources)
	      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 1, 1, &targetDescriptors.descriptorSet, 0, NULL);
	    for(source_t<SL.id>* source : allSources.template ofLayout<SL.id>()) {
	      if(source->obj->frameCreated >= frame) [[unlikely]] continue;
	      if constexpr(!TL.selfRender && SL.objectLayoutId == TL.objectLayoutId)
		if(source->objectId == target.objectId) [[unlikely]]
		  continue;
	      if(!source->template stepEnabled<RP.id>() || !source->template stepEnabled<GSR.id>()) [[unlikely]] continue;
	      size_t frameMod = frame % source_t<SL.id>::maxFrameswap;
	      auto& descriptorBundle = source->perShaderByIdByFrame[frameMod].template get<GSR.id>();
	      prepareDescriptors<object_t<SL.objectLayoutId>::RSS, SL.resources, GSR.sourceProvidedResources,
		{ layerIdx, substep_e::render, findIdx(OD.LRS[layerIdx].renders, RP.id) }>
		(descriptorBundle, pslps.descriptorPool, *source->allObjectResources(), frameMod);
	      if constexpr(GSR.sourceProvidedResources)
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	      //for now, source must provide all vertex info
	      vk::Buffer verts[vibCount];
	      vk::DeviceSize offsets[2];
	      static_assert((vb || GSR.vertexCountOverride) ^ isMesh);//vertex data required if vertex count not given statically, unless mesh shader is to be used
	      uint32_t vertices, instances;
	      if constexpr(vb) {
		static constexpr resourceReference vbm = *findResourceReferenceToConsumer(SL.resources, vb->id); //compiler-time error: dereferencing null if the source did not provide a reference to the vertex buffer used by the shader
		static constexpr resourceSlot vbs = findById(OD.RSS, vbm.resourceSlotId);
		verts[0] = source->template get<vbm.resourceSlotId>().frameBuffer(frame + vbm.frameLatency);
		offsets[0] = vbm.subresource.offset;
		vertices = GSR.vertexCountOverride ? GSR.vertexCountOverride :
		  (vbm.subresource.length != VK_WHOLE_SIZE ? vbm.subresource.length : findById(OD.BRS, vbs.requirementId).size)
		  / sizeofUdm<vb->usage.asVertex.format>();
	      } else {
		vertices = GSR.vertexCountOverride;
	      }
	      if constexpr(ib) {
		static constexpr resourceReference ibm = *findResourceReferenceToConsumer(SL.resources, ib->id); //compiler-time error: dereferencing null if the source did not provide a reference to the instance buffer used by the shader
		static constexpr resourceSlot ibs = findById(OD.RSS, ibm.resourceSlotId);
		verts[vibCount-1] = source->template get<ibm.resourceSlotId>().frameBuffer(frame + ibm.frameLatency);
		offsets[vibCount-1] = ibm.subresource.offset;
		instances = GSR.instanceCountOverride ? GSR.instanceCountOverride :
		  ibm.subresource.length != VK_WHOLE_SIZE ? ibm.subresource.length :
		  findById(OD.BRS, ibs.requirementId).size / sizeofUdm<ib->usage.asVertex.format>();
	      } else {
		//but instances defaults to 1
		instances = GSR.instanceCountOverride ? GSR.instanceCountOverride : 1;
	      }
	      if constexpr(vibCount) {
		static_assert(!isMesh,
			      "mesh shaders must not have a vertex buffer");
		cmd.bindVertexBuffers(0, vibCount, verts, offsets);
	      }
	      vk::Buffer indirectBuffer, indirectCountBuffer;
	      vk::DeviceSize indirectOffset, countOffset;
	      uint32_t drawCount;//for indirect, also maxDrawCount for indirectCount
	      static constexpr uint32_t indirectStride = isMesh ?
		sizeof(vk::DrawMeshTasksIndirectCommandEXT) : sizeof(vk::DrawIndirectCommand);
	      if constexpr(indirectConsumer) {
		static constexpr resourceReference ibm = *findResourceReferenceToConsumer(SL.resources, indirectConsumer->id);
		static constexpr resourceSlot ibs = findById(OD.RSS, ibm.resourceSlotId);
		indirectBuffer = source->template get<ibm.resourceSlotId>().frameBuffer(frame + ibm.frameLatency);
		indirectOffset = ibm.subresource.offset;
		drawCount = (ibm.subresource.length != VK_WHOLE_SIZE ? ibm.subresource.length :
			     findById(OD.BRS, ibs.requirementId).size - indirectOffset) / indirectStride;
		if constexpr(countConsumer) {
		  static constexpr resourceReference cbm = *findResourceReferenceToConsumer(SL.resources, countConsumer->id);
		  indirectCountBuffer = source->template get<cbm.resourceSlotId>().frameBuffer(frame + cbm.frameLatency);
		  countOffset = cbm.subresource.offset;
		}
	      } else {
		static_assert(countConsumer == NULL, "count buffer not allowed without indirect buffer");
	      }
	      if constexpr(indexConsumer) {//indexed draw of vertex shader
		static_assert(containsStage<GSR.modules, vk::ShaderStageFlagBits::eVertex>() && vb,
			      "index buffers only usable with vertex buffers");
		static constexpr resourceReference ibm = *findResourceReferenceToConsumer(SL.resources, indexConsumer->id);
		static constexpr resourceSlot ibs = findById(OD.RSS, ibm.resourceSlotId);
		static constexpr bufferRequirements ibr = findById(OD.BRS, ibs.requirementId);
		cmd.bindIndexBuffer(source->template get<ibm.resourceSlotId>().frameBuffer(frame + ibm.frameLatency),
				    ibm.subresource.offset, ibr.indexBufferType);
		vertices = (GSR.vertexCountOverride ? GSR.vertexCountOverride : ibr.size) /
		  sizeofIndexType(ibr.indexBufferType);
		if constexpr(countConsumer) {
		  cmd.drawIndexedIndirectCount(indirectBuffer, indirectOffset, indirectCountBuffer, countOffset,
					       drawCount, indirectStride);
		  // WARN("Drew indexed indirect count");
		} else if constexpr(indirectConsumer) {
		  cmd.drawIndexedIndirect(indirectBuffer, indirectOffset, drawCount, indirectStride);
		  // WARN("Drew indexed indirect ", drawCount);
		} else {
		  cmd.drawIndexed(vertices, instances, 0, 0, 0);
		  // WARN("Drew indexed ", instances, " instances of ", vertices, " verticies from nested target-source (", TL.id, "-", SL.id, ")");
		}
	      } else if constexpr(containsStage<GSR.modules, vk::ShaderStageFlagBits::eVertex>()) {//non-indexed draw of vertex shader
		if constexpr(countConsumer) {
		  cmd.drawIndirectCount(indirectBuffer, indirectOffset, indirectCountBuffer, countOffset,
					drawCount, indirectStride);
		  // WARN("Drew indirect count");
		} else if constexpr(indirectConsumer) {
		  cmd.drawIndirect(indirectBuffer, indirectOffset, drawCount, indirectStride);
		  // WARN("Drew indirect ", drawCount);
		} else {
		  cmd.draw(vertices, instances, 0, 0);
		  // WARN("Drew ", instances, " instances of ", vertices, " verticies from nested target-source (", TL.id, "-", SL.id, ")");
		}
	      } else {//mesh shader
		static_assert(isMesh, "invalid shader type");
		if constexpr(countConsumer) {
		  cmd.drawMeshTasksIndirectCountEXT(indirectBuffer, indirectOffset, indirectCountBuffer, countOffset,
						    drawCount, indirectStride);
		  // WARN("Drew mesh indirect count");
		} else if constexpr(indirectConsumer) {
		  cmd.drawMeshTasksIndirectEXT(indirectBuffer, indirectOffset, drawCount, indirectStride);
		  // WARN("Drew mesh indirect ", drawCount);
		} else {
		  // WARN("Drew mesh ", GSR.meshGroupCountX, ", ", GSR.meshGroupCountY, ", ", GSR.meshGroupCountZ);
		  static_assert(GSR.meshGroupCountX * GSR.meshGroupCountY * GSR.meshGroupCountZ > 0);
		  cmd.drawMeshTasksEXT(GSR.meshGroupCountX, GSR.meshGroupCountY, GSR.meshGroupCountZ);
		}
	      }
	      // if(frame == 10)
	      // 	WARN("Drew ", vertices, " verts from nested shader ", GSR.id);
	    }
	  // } else {
	  //   WARN("skipping shader ", GSR.id, " for source ", SL.id);
	  }
	}
	recordRenders_l5<layerIdx, TL, RP, GSR, SLS.sub(1)>(target, ptl, ptlps, targetDescriptors, rp, cmd);
      }
    };

    template<size_t layerIdx, renderPassRequirements RP, graphicsShaderRequirements GSR>
    inline void recordRenders_targetOnly(auto& target, perTargetLayout& ptl, perTargetLayoutPerShader& ptlps, descriptorUpdateData_t<GSR.targetProvidedResources.len>& targetDescriptors, vk::RenderPass rp, vk::CommandBuffer cmd) {
      PROFILEME;
      //for now, target only rendering must not supply a vertex or instance buffer, so vert count must come from an override
      shaderInstance& shaderInstance = ptlps.targetOnlyShader;
      if(!shaderInstance.pipeline) [[unlikely]] {
	PROFILEME_MSG("pipeline creation, target only");
	if(shaderInstance.pipelineLayout == NULL) {
	  static constexpr vk::DescriptorSetLayoutCreateInfo dslcis[] = {
	    descriptorPoolPool<GSR.targetProvidedResources, OD.GPUID>::dslci
	  };
	  shaderInstance.pipelineLayout = dev->getPipelineLayout<dslcis>();
	}
	vk::PipelineShaderStageCreateInfo stages[GSR.modules.len];
	createModules<GSR.modules, OD.GPUID>(stages);
	static constexpr vk::PipelineVertexInputStateCreateInfo verts { {}, 0, NULL, 0, NULL };
	static constexpr vk::PipelineInputAssemblyStateCreateInfo assembly { {}, GSR.topology, false };
	//TODO tessellation
	//viewport and scissors are dynamic so we don't need to rebuild the pipe when the target size changes
	static constexpr vk::PipelineViewportStateCreateInfo vp = { {}, 1, NULL, 1, NULL };
	static constexpr vk::PipelineRasterizationStateCreateInfo raster = { {}, false, false, vk::PolygonMode::eFill, GSR.cullMode, GSR.windCounterClockwise ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise, false, 0, 0, 0, 1.0f };
	static constexpr vk::PipelineMultisampleStateCreateInfo multisample = { {}, vk::SampleCountFlagBits::e1, 0, 0, NULL, 0, 0 };
	// static constexpr vk::StencilOpState stencilOp = {vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep,
	//   vk::CompareOp::eAlways, 0, 0, 0 };
	static constexpr vk::PipelineDepthStencilStateCreateInfo depth = { {}, RP.depth != NONE, GSR.depthWrite, GSR.depthCompareMode };//not set: depth bounds and stencil test stuff
	static constexpr vk::PipelineColorBlendStateCreateInfo blend = { {}, false, vk::LogicOp::eNoOp, GSR.blend.len, GSR.blend.data, { 1, 1, 1, 1 } };
	static_assert(GSR.blend.len == RP.color.len || RP.color.len == 0, "must match unless there are no colors");
	//default blend is a single sensible color blend state and we want to accept that even if there are no color attachments
	static constexpr vk::DynamicState dynamics[] = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
	static constexpr vk::PipelineDynamicStateCreateInfo dynamic = { {}, 2, dynamics };
	vk::GraphicsPipelineCreateInfo gpci { {}, GSR.modules.len, stages, &verts, &assembly, /*tesselation*/ NULL, &vp, &raster, &multisample, &depth, &blend, &dynamic, shaderInstance.pipelineLayout, rp, /*subpass idx*/ 0 };//not set: derivitive pipeline stuff
	VK_ASSERT(dev->getVkDevice().createGraphicsPipelines(dev->getPipelineCache(), 1, &gpci, ALLOCCB, &shaderInstance.pipeline), "Failed to create graphics pipeline ", GSR.id);
      }
      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderInstance.pipeline);
      if constexpr(GSR.targetProvidedResources)
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 0, 1, &targetDescriptors.descriptorSet, 0, NULL);
      static_assert(GSR.vertexCountOverride > 0);
      static_assert(containsStage<GSR.modules, vk::ShaderStageFlagBits::eVertex>(),
		    "vertex shader required. Mesh shaders not yet implemented for target-only rendering");
      cmd.draw(GSR.vertexCountOverride, GSR.instanceCountOverride ? GSR.instanceCountOverride : 1, 0, 0);
      // if(frame == 10)
      // 	WARN("Drew ", GSR.vertexCountOverride, " from target only shader ", GSR.id);
      //TODO more flexibility with draw. Allow source layout to ask for multi-draw, indexed, indirect etc. Allow (dynamic) less than the whole buffer.
    };

    template<size_t layerIdx, targetLayout TL, renderPassRequirements RP, literalList<graphicsShaderRequirements> GSRS>
    inline void recordRenders_l4(auto& target, perTargetLayout& ptl, vk::RenderPass rp, vk::CommandBuffer cmd) {
      if constexpr(GSRS.len) {
	{
	  PROFILEME;
	  static constexpr graphicsShaderRequirements GSR = GSRS[0];
	  static_assert(containsStage<GSR.modules, vk::ShaderStageFlagBits::eMeshEXT>() ^
			containsStage<GSR.modules, vk::ShaderStageFlagBits::eVertex>(),
			"graphics shaders must have a vertex or a mesh shader but not both");
	  if constexpr(satisfies(TL.resources, GSR.targetProvidedResources)) {
	    if(target.template stepEnabled<RP.id>() && target.template stepEnabled<GSRS[0].id>()) [[likely]] {
	      // WARN("Begin shader ", GSR.id, " using target ", TL.id);
	      size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	      auto& descriptorBundle = target.perShaderByIdByFrame[frameMod].template get<GSR.id>();
	      perTargetLayoutPerShader& ptlps = ptl.perShader[GSR.id];
	      prepareDescriptors<object_t<TL.objectLayoutId>::RSS, TL.resources, GSR.targetProvidedResources,
		{ layerIdx, substep_e::render, findIdx(OD.LRS[layerIdx].renders, RP.id) }>
		(descriptorBundle, ptlps.descriptorPool, *target.allObjectResources(), frameMod);
	      if constexpr(GSR.sourceProvidedResources.len)
		recordRenders_l5<layerIdx, TL, RP, GSR, OD.SLS>(target, ptl, ptlps, descriptorBundle, rp, cmd);
	      else
		recordRenders_targetOnly<layerIdx, RP, GSR>(target, ptl, ptlps, descriptorBundle, rp, cmd);
	    }
	  }
	}
	recordRenders_l4<layerIdx, TL, RP, GSRS.sub(1)>(target, ptl, rp, cmd);
      }
    };

    template<renderPassRequirements RP, targetLayout TL>
    inline vk::Rect2D getRenderSize(auto& target) {
      if constexpr(RP.depth == NONE) {
	static constexpr const resourceReference* colorRR = findResourceReferenceToConsumer(TL.resources, RP.color[0]);
	return target.template get<colorRR->resourceSlotId>().getSizeRect(frame + colorRR->frameLatency);
      } else {
	static constexpr const resourceReference* depthRR = findResourceReferenceToConsumer(TL.resources, RP.depth);
	return target.template get<depthRR->resourceSlotId>().getSizeRect(frame + depthRR->frameLatency);
      }
    };

    template<targetLayout TL, literalList<uint64_t> IDS> inline bool anyOutdated(uint64_t fbUpdate, auto& target) {
      if constexpr(IDS && IDS[0] != NONE) {
	static constexpr const resourceReference RR = *findResourceReferenceToConsumer(TL.resources, IDS[0]);
	return fbUpdate < target.template get<RR.resourceSlotId>().frameUpdated(frame + RR.frameLatency) ||
	  anyOutdated<TL, IDS.sub(1)>(fbUpdate, target);
      }
      return false;
    };

    template<targetLayout TL, literalList<uint64_t> IDS, uint32_t idx> inline void updateIfNeeded(frameBufferBundle& fbb, auto& target) {
      if constexpr(IDS && IDS[0] != NONE) {
	static constexpr const resourceReference RR = *findResourceReferenceToConsumer(TL.resources, IDS[0]);
	auto& img = target.template get<RR.resourceSlotId>();
	bool outdated = img.frameUpdated(frame + RR.frameLatency);
	if(outdated)
	  getActiveGarbageCollector().push(fbb.attachments[idx]);
	if(!fbb.fb || outdated) [[likely]]
	  fbb.attachments[idx] = img.template createView<RR.subresource.viewType, getSubresource(findById(OD.RSS, RR.resourceSlotId).requirementId, RR)>(frame + RR.frameLatency);
	updateIfNeeded<TL, IDS.sub(1), idx+1>(fbb, target);
      }
    };

    template<targetLayout TL, literalList<uint64_t> IDS> inline void validateFbSizes(const vk::Rect2D& size, auto& target) {
#ifdef DEBUG
      if constexpr(IDS && IDS[0] != NONE) {
	static constexpr const resourceReference RR = *findResourceReferenceToConsumer(TL.resources, IDS[0]);
	ASSERT_TRAP(size == target.template get<RR.resourceSlotId>().getSizeRect(frame + RR.frameLatency),
		    "framebuffer attachment size mismatch");
	validateFbSizes<TL, IDS.sub(1)>(size, target);
      }
#endif
    };

    template<size_t layerIdx, renderPassRequirements RP, targetLayout TL>
    inline void recordRenders_l3(auto& target, perTargetLayout& ptl, vk::CommandBuffer cmd) {
      static constexpr const resourceReference* depthRR = findResourceReferenceToConsumer(TL.resources, RP.depth);
      static_assert(RP.color || RP.depth != NONE, "render pass must have a depth or at least one color attachment");
      if constexpr(satisfies(TL.resources, RP.color) &&
		   satisfies(TL.resources, RP.input) &&
		   (depthRR != NULL || RP.depth == NONE)) {
	PROFILEME;
	vk::RenderPass& rp = ptl.rpByRequirementId[RP.id];
	if(!rp) [[unlikely]] {
	  VK_ASSERT(dev->getVkDevice().createRenderPass(&renderPassInfoBundle<layerIdx, RP, TL>::rpci, ALLOCCB, &rp),
		    "failed to create render pass ", TL.id, " ", RP.id);
	}
	frameBufferBundle& fbb = target.fbByRpIdByFrame[RP.id][frame % target_t<TL.id>::maxFrameswap];
	vk::Rect2D size = getRenderSize<RP, TL>(target);
	static constexpr uint32_t attachmentCount = renderPassInfoBundle<layerIdx, RP, TL>::attachments.len;
	if(!fbb.fb ||
	   anyOutdated<TL, RP.input>(fbb.lastUpdatedFrame, target) ||
	   anyOutdated<TL, RP.color>(fbb.lastUpdatedFrame, target) ||
	   anyOutdated<TL, RP.depth>(fbb.lastUpdatedFrame, target)) [[unlikely]] {
	  if(fbb.fb) {
	    getActiveGarbageCollector().push(fbb.fb);
	    ASSERT_TRAP(fbb.fbci.attachmentCount == attachmentCount,
			"fbb attachment count shifted (should never happen)");
	  } else {
	    ASSERT_TRAP(!fbb.attachments, "fbb did not exist but attechment collection did?");
	    fbb.attachments = std::make_unique<vk::ImageView[]>(attachmentCount);
	    fbb.fbci.attachmentCount = attachmentCount;
	    fbb.fbci.pAttachments = fbb.attachments.get();
	  }
	  fbb.fbci.renderPass = rp;
	  fbb.fbci.width = size.extent.width;
	  fbb.fbci.height = size.extent.height;
	  fbb.fbci.layers = 1;
	  validateFbSizes<TL, RP.color>(size, target);
	  validateFbSizes<TL, RP.input>(size, target);
	  validateFbSizes<TL, RP.depth>(size, target);
	  updateIfNeeded<TL, RP.color, 0>(fbb, target);
	  updateIfNeeded<TL, RP.input, renderPassInfoBundle<layerIdx, RP, TL>::firstInput>(fbb, target);
	  updateIfNeeded<TL, RP.depth, renderPassInfoBundle<layerIdx, RP, TL>::firstDepth>(fbb, target);
	  VK_ASSERT(dev->getVkDevice().createFramebuffer(&fbb.fbci, ALLOCCB, &fbb.fb), "failed to create framebuffer");
	  fbb.lastUpdatedFrame = frame;
	}
	//MAYBE multiview
	vk::Viewport viewport = { 0, 0, (float)size.extent.width, (float)size.extent.height, 0.0f, 1.0f };
	cmd.setViewport(0, 1, &viewport);
	cmd.setScissor(0, 1, &size);
	static constexpr copyableArray<vk::ClearValue, attachmentCount> clears = [](size_t i){
	  return i < RP.color.len ? vk::ClearValue(RP.clearColorValue) : vk::ClearValue(RP.clearDepthValue);
	};
	vk::RenderPassBeginInfo rpBegin(rp, fbb.fb, size, attachmentCount, clears.ptr());
	cmd.beginRenderPass(&rpBegin, vk::SubpassContents::eInline);
	// WARN("RP begin");
	recordRenders_l4<layerIdx, TL, RP, RP.shaders>(target, ptl, rp, cmd);
	cmd.endRenderPass();
      // } else {
      // 	if(frame == 1) [[unlikely]] WARN("Warning: skipping rp ", RP.id, " on TL ", TL.id);
      }
    };

    template<size_t layerIdx, renderPassRequirements RP, literalList<targetLayout> TLS>
    inline void recordRenders_l2(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	PROFILEME;
	static constexpr targetLayout TL = TLS[0];
	perTargetLayout& ptl = od.perTL[TL.id];
	for(auto* target : allTargets.template ofLayout<TL.id>()) {
	  if(target->obj->frameCreated >= frame) [[unlikely]] continue;
	  recordRenders_l3<layerIdx, RP, TL>(*target, ptl, cmd);
	}
	recordRenders_l2<layerIdx, RP, TLS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx, literalList<uint64_t> RPIDS>
    inline void recordRenders(vk::CommandBuffer cmd) {
      if constexpr(RPIDS.len) {
	PROFILEME;
	static constexpr renderPassRequirements RP = findById(OD.RPRS, RPIDS[0]);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::render, .passOrShaderIdx = findId(OD.RPRS, RP.id) }>(cmd);
	recordRenders_l2<layerIdx, RP, OD.TLS>(cmd);
	recordRenders<layerIdx, RPIDS.sub(1)>(cmd);
      }
    };

    template<computeShaderRequirements CS, uint64_t TID, uint64_t SID> static inline void getWorkgroupSize(vk::Extent3D& workgroupSize, target_t<TID>* target, source_t<SID>* source, uint64_t frameMod) {
      PROFILEME;
      static constexpr resourceSlot RS = findById(OD.RSS, CS.primaryOutputSlotId);
      if constexpr(containsId(OD.IRS, RS.requirementId)) {//is image
	vk::Extent3D imageSize;
	if constexpr(TID != NONE && findResourceReferenceToConsumer(findById(OD.TLS, TID).resources, RS.id))
	  imageSize = target->template get<RS.id>().getSizeExtent(frameMod);
	else
	  imageSize = source->template get<RS.id>().getSizeExtent(frameMod);
	workgroupSize.width = (imageSize.width - 1) / CS.strideX + 1;
	workgroupSize.height = (imageSize.height - 1) / CS.strideY + 1;
	workgroupSize.depth = (imageSize.depth - 1) / CS.strideZ + 1;
      } else {
	workgroupSize.width = (findById(OD.BRS, RS.requirementId).size - 1) / CS.strideX + 1;
	workgroupSize.height = 1;
	workgroupSize.depth = 1;
      }
    };

    template<size_t layerIdx, computeShaderRequirements CS, literalList<sourceLayout> SLS>
    inline void recordComputeDispatches_sourceOnly(vk::CommandBuffer cmd) {
      if constexpr(SLS.len) {
	{
	  PROFILEME;
	  static constexpr sourceLayout SL = SLS[0];
	  if constexpr(satisfies(SL.resources, CS.sourceProvidedResources)) {
	    for(source_t<SL.id>* source : allSources.template ofLayout<SL.id>()) {
	      if(source->obj->frameCreated >= frame) [[unlikely]] continue;
	      if(!source->template stepEnabled<CS.id>()) [[unlikely]] continue;
	      perSourceLayoutPerShader& pslps = od.perSL[SL.id].perShader[CS.id];
	      size_t frameMod = frame % source_t<SL.id>::maxFrameswap;
	      auto& descriptorBundle = source->perShaderByIdByFrame[frameMod].template get<CS.id>();
	      prepareDescriptors<object_t<SL.objectLayoutId>::RSS, SL.resources, CS.sourceProvidedResources,
		{ layerIdx, substep_e::compute, findIdx(OD.LRS[layerIdx].computeShaders, CS.id) }>
		(descriptorBundle, pslps.descriptorPool, *source->allObjectResources(), frameMod);
	      if(!pslps.sourceOnlyShader.pipeline) [[unlikely]] {
		PROFILEME_MSG("compute pipeline creation, source only");
		pslps.sourceOnlyShader.pipelineLayout = dev->getPipelineLayout<descriptorPoolPool<CS.sourceProvidedResources, OD.GPUID>::dslci>();
		static_assert(CS.module->stage == vk::ShaderStageFlagBits::eCompute);
		vk::ComputePipelineCreateInfo ci;
		shaderFactory<CS.module, OD.GPUID>::create(&ci.stage);
		ci.layout = pslps.sourceOnlyShader.pipelineLayout;
		VK_ASSERT(dev->getVkDevice().createComputePipelines(dev->getPipelineCache(), 1, &ci, ALLOCCB, &pslps.sourceOnlyShader.pipeline), "failed to create compute pipeline");
	      }
	      cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pslps.sourceOnlyShader.pipeline);
	      if constexpr(CS.sourceProvidedResources)
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pslps.sourceOnlyShader.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	      vk::Extent3D workgroupSize;
	      getWorkgroupSize<CS, NONE, SL.id>(workgroupSize, NULL, source, frameMod);
	      cmd.dispatch(workgroupSize.width, workgroupSize.height, workgroupSize.depth);
	    }
	  }
	}
	recordComputeDispatches_sourceOnly<layerIdx, CS, SLS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx, computeShaderRequirements CS, literalList<targetLayout> TLS>
    inline void recordComputeDispatches_targetOnly(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	{
	  PROFILEME;
	  static constexpr targetLayout TL = TLS[0];
	  if constexpr(satisfies(TL.resources, CS.targetProvidedResources)) {
	    for(target_t<TL.id>* target : allTargets.template ofLayout<TL.id>()) {
	      if(target->obj->frameCreated >= frame) [[unlikely]] continue;
	      if(!target->template stepEnabled<CS.id>()) [[unlikely]] continue;
	      perTargetLayoutPerShader& ptlps = od.perTL[TL.id].perShader[CS.id];
	      size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	      auto& descriptorBundle = target->perShaderByIdByFrame[frameMod].template get<CS.id>();
	      prepareDescriptors<object_t<TL.objectLayoutId>::RSS, TL.resources, CS.targetProvidedResources,
		{ layerIdx, substep_e::compute, findIdx(OD.LRS[layerIdx].computeShaders, CS.id) }>
		(descriptorBundle, ptlps.descriptorPool, *target->allObjectResources(), frameMod);
	      if(!ptlps.targetOnlyShader.pipeline) [[unlikely]] {
		PROFILEME_MSG("compute pipeline creation, target only");
		ptlps.targetOnlyShader.pipelineLayout = dev->getPipelineLayout<descriptorPoolPool<CS.targetProvidedResources, OD.GPUID>::dslci>();
		static_assert(CS.module->stage == vk::ShaderStageFlagBits::eCompute);
		vk::ComputePipelineCreateInfo ci;
		shaderFactory<CS.module, OD.GPUID>::create(&ci.stage);
		ci.layout = ptlps.targetOnlyShader.pipelineLayout;
		VK_ASSERT(dev->getVkDevice().createComputePipelines(dev->getPipelineCache(), 1, &ci, ALLOCCB, &ptlps.targetOnlyShader.pipeline), "failed to create compute pipeline");
	      }
	      cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ptlps.targetOnlyShader.pipeline);
	      if constexpr(CS.targetProvidedResources)
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ptlps.targetOnlyShader.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	      vk::Extent3D workgroupSize;
	      getWorkgroupSize<CS, TL.id, NONE>(workgroupSize, target, NULL, frameMod);
	      cmd.dispatch(workgroupSize.width, workgroupSize.height, workgroupSize.depth);
	    }
	  }
	}
	recordComputeDispatches_targetOnly<layerIdx, CS, TLS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx, computeShaderRequirements CS, targetLayout TL, literalList<sourceLayout> SLS>
    inline void recordComputeDispatches_nested_l2(target_t<TL.id>* target, perTargetLayoutPerShader& ptlps, descriptorUpdateData_t<CS.targetProvidedResources.len>& perShader, vk::CommandBuffer cmd) {
      if constexpr(SLS.len) {
	{
	  PROFILEME;
	  static constexpr sourceLayout SL = SLS[0];
	  if constexpr(satisfies(SL.resources, CS.sourceProvidedResources)) {
	    shaderInstance& shaderInstance = ptlps.perSL[SL.id];
	    perSourceLayoutPerShader& pslps = od.perSL[SL.id].perShader[CS.id];
	    if(!pslps.descriptorPool) [[unlikely]]
	      pslps.descriptorPool.reset(new descriptorPoolPool<CS.sourceProvidedResources, OD.GPUID>());
	    if(!shaderInstance.pipeline) [[unlikely]] {
	      PROFILEME_MSG("compute pipeline creation, nested");
	      if(shaderInstance.pipelineLayout == NULL) [[unlikely]] {
		static constexpr vk::DescriptorSetLayoutCreateInfo dslcis[] = {
		  descriptorPoolPool<CS.sourceProvidedResources, OD.GPUID>::dslci,
		  descriptorPoolPool<CS.targetProvidedResources, OD.GPUID>::dslci
		};
		shaderInstance.pipelineLayout = dev->getPipelineLayout<dslcis>();
	      }
	      static_assert(CS.module->stage == vk::ShaderStageFlagBits::eCompute);
	      vk::ComputePipelineCreateInfo ci;
	      shaderFactory<CS.module, OD.GPUID>::create(&ci.stage);
	      ci.layout = shaderInstance.pipelineLayout;
	      VK_ASSERT(dev->getVkDevice().createComputePipelines(dev->getPipelineCache(), 1, &ci, ALLOCCB, &shaderInstance.pipeline), "failed to create compute pipeline");
	    }
	    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, shaderInstance.pipeline);
	    if constexpr(CS.targetProvidedResources)
	      cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shaderInstance.pipelineLayout, 1, 1, &perShader.descriptorSet, 0, NULL);
	    for(source_t<SL.id>* source : allSources.template ofLayout<SL.id>()) {
	      if(source->obj->frameCreated >= frame) [[unlikely]] continue;
	      if constexpr(!TL.selfRender && SL.objectLayoutId == TL.objectLayoutId)
		if(source->objectId == target->objectId) [[unlikely]]
		  continue;
	      if(!source->template stepEnabled<CS.id>()) [[unlikely]] continue;
	      size_t frameMod = frame % source_t<SL.id>::maxFrameswap;
	      auto& descriptorBundle = source->perShaderByIdByFrame[frameMod].template get<CS.id>();
	      prepareDescriptors<object_t<SL.objectLayoutId>::RSS, SL.resources, CS.sourceProvidedResources,
		{ layerIdx, substep_e::compute, findIdx(OD.LRS[layerIdx].computeShaders, CS.id) }>
		(descriptorBundle, pslps.descriptorPool, source->resources, frameMod);
	      if constexpr(CS.sourceProvidedResources)
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shaderInstance.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	      vk::Extent3D workgroupSize;
	      getWorkgroupSize<CS, TL.id, SL.id>(workgroupSize, target, source, frameMod);
	      cmd.dispatch(workgroupSize.width, workgroupSize.height, workgroupSize.depth);
	    }
	  }
	}
	recordComputeDispatches_nested_l2<layerIdx, CS, TL, SLS.sub(1)>(target, ptlps, perShader, cmd);
      }
    };

    template<size_t layerIdx, computeShaderRequirements CS, literalList<targetLayout> TLS, literalList<sourceLayout> SLS>
    inline void recordComputeDispatches_nested(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	{
	  PROFILEME;
	  static constexpr targetLayout TL = TLS[0];
	  if constexpr(satisfies(TL.resources, CS.targetProvidedResources)) {
	    for(target_t<TL.id>* target : allTargets.template ofLayout<TL.id>()) {
	      if(target->obj->frameCreated >= frame) [[unlikely]] continue;
	      if(!target->template stepEnabled<CS.id>()) [[unlikely]] continue;
	      perTargetLayoutPerShader& ptlps = od.perTL[TL.id].perShader[CS.id];
	      size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	      auto& descriptorBundle = target->perShaderByIdByFrame[frameMod].template get<CS.id>();
	      prepareDescriptors<object_t<TL.objectLayoutId>::RSS, TL.resources, CS.targetProvidedResources,
		{ layerIdx, substep_e::compute, findIdx(OD.LRS[layerIdx].computeShaders, CS.id) }>
		(descriptorBundle, ptlps.descriptorPool, target->resources, frameMod);
	      recordComputeDispatches_nested_l2<layerIdx, CS, TL, SLS>(target, ptlps, descriptorBundle, cmd);
	    }
	  }
	}
	recordComputeDispatches_nested<layerIdx, CS, TLS.sub(1), SLS>(cmd);
      }
    };

    template<size_t layerIdx, literalList<uint64_t> CSS>
    inline void recordComputeDispatches(vk::CommandBuffer cmd) {
      if constexpr(CSS.len) {
	{
	  PROFILEME;
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::compute, .passOrShaderIdx = findId(OD.CSRS, CSS[0]) }>(cmd);
	  static constexpr computeShaderRequirements CS = findById(OD.CSRS, CSS[0]);
	  if constexpr(CS.targetProvidedResources.len == 0)
	    recordComputeDispatches_sourceOnly<layerIdx, CS, OD.SLS>(cmd);
	  else if constexpr(CS.sourceProvidedResources.len == 0)
	    recordComputeDispatches_targetOnly<layerIdx, CS, OD.TLS>(cmd);
	  else
	    recordComputeDispatches_nested<layerIdx, CS, OD.TLS, OD.SLS>(cmd);
	}
	recordComputeDispatches<layerIdx, CSS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx> inline void renderFrom(vk::CommandBuffer cmd) {
      if constexpr(layerIdx < OD.LRS.len) {
	{
	  PROFILEME;
	  static constexpr layerRequirements LR = OD.LRS[layerIdx];
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier0 }>(cmd);
	  recordCopies<layerIdx, LR.copies>(cmd);
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier1 }>(cmd);
	  recordClears<LR.clears>(cmd);
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier2 }>(cmd);
	  recordRenders<layerIdx, LR.renders>(cmd);
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier3 }>(cmd);
	  recordComputeDispatches<layerIdx, LR.computeShaders>(cmd);
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier4 }>(cmd);
	}
	renderFrom<layerIdx+1>(cmd);
      }
    };

    template<literalList<objectLayout> OLS> inline void doPrerender(vk::CommandBuffer cmd) {
      if constexpr(OLS.len) {
	{
	  PROFILEME;
	  constexpr objectLayout OL = OLS[0];
	  for(auto* object : allObjects.template ofLayout<OL.id>()) {
	    if(object->frameCreated >= frame) [[unlikely]] continue;
	    object->preRender(cmd);
	  }
	}
	doPrerender<OLS.sub(1)>(cmd);
      }
    };

    template<literalList<objectLayout> OLS> inline void doPostrender(vk::SemaphoreSubmitInfo& renderWaitSem) {
      if constexpr(OLS.len) {
	{
	  PROFILEME;
	  constexpr objectLayout OL = OLS[0];
	  if constexpr(object_t<OL.id>::hasWindow) {
	    for(auto* object : allObjects.template ofLayout<OL.id>()) {
	      if(object->frameCreated >= frame) [[unlikely]] continue;
	      object->postRender(renderWaitSem);
	    }
	  }
	}
	doPostrender<OLS.sub(1)>(renderWaitSem);
      }
    };

    void waitForFrame(uint64_t frame, bool isJoining = false, uint64_t timeoutNS = 10000000000) {//default = 10 seconds
      PROFILEME;
      if(frame == 0) [[unlikely]] return;//first frame is 1
      ASSERT_TRAP(frame < this->frame, "attempted to wait future frame");
// #ifdef DEBUG //this fires on prepareDescriptors when spawning objects with single frameswap. Removing because single frameswap with occassional waiting on the last frame is preferable to having multiple identical buffers
//       if(frame == this->frame - 1 && !isJoining) [[unlikely]]
// 	WARN("WARNING: waiting on the previous frame is a bad idea. Need more frameswap? frame: ", frame, " this->frame: ", this->frame, " cmdFrameswapCount: ", cmdFrameswapCount);
// #endif
      if(frame < this->frame - cmdFrameswapCount) [[unlikely]] {
#ifdef WITE_DEBUG_FENCES
	WARN("(not) waiting on ancient frame ", frame, " current frame is ", this->frame);
#endif
	return;
      }
      size_t fenceIdx = frame % cmdFrameswapCount;
#ifdef WITE_DEBUG_FENCES
      WARN("waiting on fence ", fenceIdx, " for frame ", frame, " current frame is ", this->frame);
#endif
      ASSERT_TRAP(frame < this->frame, "attempted to wait a current or future frame: ", frame, "; current is ", this->frame);
      VK_ASSERT(dev->getVkDevice().waitForFences(1, &fences[fenceIdx], false, timeoutNS), "Frame timeout");
#ifdef WITE_DEBUG_FENCES
      WARN("received signal on fence ", fenceIdx, " for frame ", frame, " current frame is ", this->frame);
#endif
    };

    void gcAll() {
      for(auto& gc : collectors)
	gc.collect();
    };

    void join() {//as in thread join, block until all renderings are caught up, usually before exiting
      scopeLock lock(&mutex);//prevents more frames from being queued
      waitForFrame(frame-1, true);
      gcAll();
    };

    uint64_t render() {//returns frame number
      PROFILEME;
      //TODO make some secondary cmds
      //TODO cache w/ dirty check the cmds
      scopeLock lock(&mutex);
      if(frame > cmdFrameswapCount)
	waitForFrame(frame - cmdFrameswapCount);
      getActiveGarbageCollector().collect();
      vk::CommandBuffer cmd = primaryCmds[frame % cmdFrameswapCount];
      vk::CommandBufferBeginInfo begin { vk::CommandBufferUsageFlagBits::eOneTimeSubmit };//TODO remove thsi flag when caching is implemented
      VK_ASSERT(cmd.begin(&begin), "failed to begin a command buffer");
      doPrerender<OD.OLS>(cmd);
      renderFrom<0>(cmd);
      VK_ASSERT(cmd.end(), "failed to end a command buffer");
      size_t fenceIdx = frame % cmdFrameswapCount;
      vk::Fence fence = fences[fenceIdx];
#ifdef WITE_DEBUG_FENCES
      WARN("queuing signal of fence ", fenceIdx, " to protect cmd ", cmd);
#endif
      VK_ASSERT(dev->getVkDevice().resetFences(1, &fence), "Failed to reset fence (?how?)");
      vk::SemaphoreSubmitInfo waitSem(semaphore, frame-1, vk::PipelineStageFlagBits2::eAllCommands),
	signalSem(semaphore, frame, vk::PipelineStageFlagBits2::eAllCommands);
      vk::CommandBufferSubmitInfo cmdSubmitInfo(cmd);
      vk::SubmitInfo2 submit({}, frame ? 1 : 0, &waitSem, 1, &cmdSubmitInfo, 1, &signalSem);
      // WARN("submit");
      {
	scopeLock lock(dev->getQueueMutex());
	VK_ASSERT(dev->getQueue().submit2(1, &submit, fence), "failed to submit command buffer");
      }
      doPostrender<OD.OLS>(signalSem);//signaled by the render, waited by the presentation
      return frame++;
    };

    ~onion() {
      PROFILEME;
      join();
      gcAll();
      auto vkdev = dev->getVkDevice();
      vkdev.freeCommandBuffers(cmdPool, cmdFrameswapCount, primaryCmds);
      vkdev.destroy(cmdPool, ALLOCCB);
      for(size_t i = 0;i < cmdFrameswapCount;i++)
	vkdev.destroy(fences[i], ALLOCCB);
      vkdev.destroy(semaphore, ALLOCCB);
    };

  };

};
