#pragma once

#include <numeric>
#include <type_traits>

#include "window.hpp"
#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "iterableRecyclingPool.hpp"
#include "descriptorPoolPool.hpp"
#include "tempMap.hpp"

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
    static constexpr auto allLayoutIds = concat(allTargetIds, allSourceIds);

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
      scopeLock lock(&onionStaticData::allDataMutex);
      return onionStaticData::allOnionData[hash(OD)];
    };

    onion() : od(getOd()) {
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
    };

    inline garbageCollector& getActiveGarbageCollector() {
      return collectors[frame % cmdFrameswapCount];
    };

    static consteval vk::ImageSubresourceRange getSubresource(const uint64_t IRID, const resourceReference& RR) {
      if(!RR.subresource.isDefault)
	return RR.subresource.image.range;
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

    //NOTE: A source/target layout is not allowed to name two resources referencing the same usage
    static consteval const resourceReference* findResourceReferenceToSlot(literalList<resourceReference> RRS, uint64_t slotId) {
      if(slotId == NONE) return NULL;
      for(const resourceReference& RS : RRS)
	if(RS.resourceSlotId == slotId)
	  return &RS;
      return NULL;
    };

    //NOTE: A source/target layout is not allowed to name two resources referencing the same usage
    static consteval const resourceReference* findResourceReferenceToConsumer(literalList<resourceReference> RRS, uint64_t consumerId) {
      if(consumerId == NONE) return NULL;
      for(const resourceReference& RS : RRS)
	if(RS.resourceConsumerId == consumerId)
	  return &RS;
      return NULL;
    };

    static consteval resourceConsumer consumerForWindowForObject(uint64_t objectLayoutId) {
      objectLayout OL = findById(OD.OLS, objectLayoutId);
      return { OL.windowConsumerId, {}, vk::AccessFlagBits2::eTransferWrite, {} };
    };

    static consteval resourceConsumer consumerForColorAttachment(uint64_t id) {
      return { id, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eColorAttachmentWrite };
    };

    static consteval resourceConsumer consumerForDepthAttachment(uint64_t id) {
      return { id, vk::ShaderStageFlagBits::eFragment, vk::AccessFlagBits2::eDepthStencilAttachmentWrite };
    };

    static consteval std::vector<resourceAccessTime> findUsages(uint64_t resourceSlotId) {
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
	    ret.push_back({ layerIdx, substep_e::copy, { substep.src, {}, vk::AccessFlagBits2::eTransferRead }, referencesByConsumerId[substep.src].frameLatency });
	  if(referencesByConsumerId.contains(substep.dst))
	    ret.push_back({ layerIdx, substep_e::copy, { substep.dst, {}, vk::AccessFlagBits2::eTransferWrite }, referencesByConsumerId[substep.dst].frameLatency });
	}
	//clear:
	for(uint64_t substepId : layer.clears) {
	  const auto& substep = findById(OD.CLS, substepId);
	  if(referencesByConsumerId.contains(substep.id))
	    ret.push_back({ layerIdx, substep_e::clear, { substep.id, {}, vk::AccessFlagBits2::eTransferWrite }, referencesByConsumerId[substep.id].frameLatency });
	}
	//rendering:
	static_assert(std::numeric_limits<decltype(resourceBarrier::frameLatency)>::min() == 0);
	size_t shaderIdx;
	for(uint64_t passIdx = 0;passIdx < layer.renders.len;passIdx++) {
	  uint64_t passId = layer.renders[passIdx];
	  shaderIdx = 1;//first shader per pass is idx 1 so 0 can mean "before the first shader" (for attachments)
	  const auto& pass = findById(OD.RPRS, passId);
	  if(referencesByConsumerId.contains(pass.depth))
	    ret.push_back({ layerIdx, substep_e::render, consumerForDepthAttachment(pass.depth), referencesByConsumerId[pass.depth].frameLatency, passIdx, passId });
	  if(referencesByConsumerId.contains(pass.color))
	    ret.push_back({ layerIdx, substep_e::render, consumerForColorAttachment(pass.color), referencesByConsumerId[pass.color].frameLatency, passIdx, passId });
	  for(const graphicsShaderRequirements& gsr : pass.shaders) {
	    for(const resourceConsumer& srr : gsr.targetProvidedResources)
	      if(referencesByConsumerId.contains(srr.id))
		ret.push_back({ layerIdx, substep_e::render, srr, referencesByConsumerId[srr.id].frameLatency, passIdx, passId, shaderIdx, gsr.id });
	    for(const resourceConsumer& srr : gsr.sourceProvidedResources)
	      if(referencesByConsumerId.contains(srr.id))
		ret.push_back({ layerIdx, substep_e::render, srr, referencesByConsumerId[srr.id].frameLatency, passIdx, passId, shaderIdx, gsr.id });
	    shaderIdx++;
	  }
	}
	//compute:
	shaderIdx = 0;
	for(uint64_t csrId : layer.computeShaders) {
	  const computeShaderRequirements& csr = findById(OD.CSRS, csrId);
	  for(const resourceConsumer& srr : csr.targetProvidedResources)
	    if(referencesByConsumerId.contains(srr.id))
	      ret.push_back({ layerIdx, substep_e::compute, srr, referencesByConsumerId[srr.id].frameLatency, NONE, NONE, shaderIdx, csr.id });
	  for(const resourceConsumer& srr : csr.sourceProvidedResources)
	    if(referencesByConsumerId.contains(srr.id))
	      ret.push_back({ layerIdx, substep_e::compute, srr, referencesByConsumerId[srr.id].frameLatency, NONE, NONE, shaderIdx, csr.id });
	  shaderIdx++;
	}
      }
      for(auto& ol : OD.OLS)
	if(referencesByConsumerId.contains(ol.windowConsumerId))
	  ret.push_back({ OD.LRS.len-1, substep_e::post, { NONE, {}, vk::AccessFlagBits2::eTransferRead }, referencesByConsumerId[ol.windowConsumerId].frameLatency });
      std::sort(ret.begin(), ret.end());
      return ret;
    };

    static consteval std::vector<resourceBarrier> findBarriers(uint64_t resourceSlotId) {
      auto usage = findUsages(resourceSlotId);
      size_t afterIdx = 0;
      std::vector<resourceBarrier> ret;
      if(usage.size() < 2) return ret;
      resourceAccessTime before = usage[usage.size()-1];
      resourceSlot RS = findById(OD.RSS, resourceSlotId);
      while(afterIdx < usage.size()) {
	resourceBarrier rb;
	rb.before = before;
	rb.after = usage[afterIdx];
	rb.resourceSlotId = RS.id;
	rb.requirementId = RS.requirementId;
	rb.objectLayoutId = RS.objectLayoutId;
	rb.timing.layerIdx = rb.after.layerIdx;
	rb.frameLatency = rb.after.frameLatency;
	if(rb.before.layerIdx != rb.after.layerIdx || afterIdx == 0) {//prefer top of layer for barriers
	  //afterIdx == 0: edge case for only having 1 layer
	  rb.timing.substep = substep_e::barrier0;
	} else if(rb.after.substep != rb.before.substep) {//next prefer between steps
	  rb.timing.substep = (substep_e)((int)rb.after.substep - 1);
	} else if(rb.after.passId != rb.before.passId) {//barriers between render passes are also allowed
	  constexpr_assert(rb.after.substep == substep_e::render);//only render has passes
	  rb.timing.substep = rb.after.substep;
	  rb.timing.passId = rb.after.passId;
	  rb.timing.shaderId = NONE;
	} else {//otherwise wait as long as possible
	  constexpr_assert(rb.after.substep == substep_e::compute);//mid-pass barriers not supported (would need dependencies)
	  rb.timing.substep = rb.after.substep;
	  rb.timing.shaderId = rb.after.shaderId;
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
      return ret;
    };

    template<uint64_t resourceSlotId, imageRequirements IR> static consteval auto findFinalUsagePerFrame() {
      auto usages = findUsages(resourceSlotId);
      constexpr_assert(usages.size() > 0);
      copyableArray<resourceAccessTime, IR.frameswapCount> ret;
      for(auto& b : usages)
	ret[b.frameLatency] = b;
      for(size_t i = 0;i < ret.LENGTH;i++) {
	auto& b = ret[i];
	if(b.layerIdx == NONE_size) //empty frame slot when default initialized
	  for(size_t j = ret.LENGTH;j && b.layerIdx == NONE_size;j--)
	    b = ret[(i + j - 1) % ret.LENGTH];
	constexpr_assert(b.layerIdx != NONE_size);//image never used, so can't pick which layout to init to
	//MAYBE add flag to not init? But why would it be in the onion if not used
      }
      return ret;
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

    template<literalList<resourceSlot> RSS> struct mappedResourceTuple {
      static constexpr resourceSlot RS = RSS[0];
      typedef resourceTraits<RS> RT;
      RT::type data;
      mappedResourceTuple<RSS.sub(1)> rest;

      template<size_t RSID = RS.id> inline auto& at() {
	if constexpr(RSID == RS.id) {
	  if constexpr(RS.external) {
	    return *data;
	  } else {
	    return data;
	  }
	} else {
	  return rest.template at<RSID>();
	}
      };

      template<size_t RSID = RS.id> inline void set(auto* t) {
	if constexpr(RSID == RS.id) {
	  static_assert(RS.external);
	  data = t;
	} else {
	  rest.template set<RSID>(t);
	}
      };

      inline uint64_t frameLastUpdated(uint64_t currentFrame) {
	uint64_t ret = at().frameUpdated(currentFrame);
	if constexpr(RSS.len > 1) {
	  ret = max(ret, rest.frameLastUpdated(currentFrame));
	}
	return ret;
      };

      template<uint64_t FS> inline void preRender(uint64_t frame, vk::CommandBuffer cmd, garbageCollector& gc) {
	if constexpr(RT::isImage) {
	  static constexpr resourceConsumer RC = findFinalUsagePerFrame<RS, RT::IR>()[FS].usage;
	  at().template applyPendingResize<RT::IR.resizeBehavior, RC.access>(frame + FS, cmd, gc);
	  if constexpr(FS < RT::IR.frameswapCount-1)
	    preRender<FS+1>(frame, cmd, gc);
	}
      }

      inline void preRender(uint64_t frame, vk::CommandBuffer cmd, garbageCollector& gc) {
	if constexpr(RT::isImage)
	  preRender<0>(frame, cmd, gc);
	if constexpr(RSS.len > 1)
	  rest.preRender(frame, cmd, gc);
      };

      inline void trackWindowSize(uint64_t frame, vk::Extent3D& wSize) {
	if constexpr(RT::isImage && RT::IR.resizeBehavior.image.trackWindow) {
	  auto& img = at();
	  auto imgSize = img.getSizeExtent(frame);
	  if(wSize != imgSize)
	    img.resize(frame, wSize);
	}
	if constexpr(RSS.len > 1)
	  rest.trackWindowSize(frame, wSize);
      };

      inline static consteval auto initBaselineBarriers() {
	//consteval lambdas are finicky, need to be wrapped in a consteval function
	if constexpr(RT::isImage) {
	  constexpr auto finalUsagePerFrame = findFinalUsagePerFrame<RS, RT::IR>();
	  return copyableArray<vk::ImageMemoryBarrier2, RT::IR.frameswapCount>([&](size_t i) consteval {
	    return vk::ImageMemoryBarrier2 {
	      vk::PipelineStageFlagBits2::eNone,
	      vk::AccessFlagBits2::eNone,
	      toPipelineStage2(finalUsagePerFrame[i]),
	      finalUsagePerFrame[i].usage.access,
	      vk::ImageLayout::eUndefined,
	      imageLayoutFor(finalUsagePerFrame[i].usage.access),
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
	  //only images need a layout initialization, each into the layouts they will be assumed to be when first used.
	  static constexpr size_t FC = RT::IR.frameswapCount;
	  static constexpr auto baseline = initBaselineBarriers();
	  copyableArray<vk::ImageMemoryBarrier2, FC> barriers = baseline;
	  for(int64_t i = 0;i < FC;i++) {
	    barriers[i].image = data.frameImage(i + currentFrame);
	    WITE_DEBUG_IB(barriers[i], cmd);
	  }
	  vk::DependencyInfo di { {}, 0, NULL, 0, NULL, FC, barriers.ptr() };
	  cmd.pipelineBarrier2(&di);
	}
	if constexpr(RSS.len > 1)
	  rest.init(currentFrame, cmd);
      };

    };

    template<literalList<resourceSlot> RM> requires(RM.len == 0) struct mappedResourceTuple<RM> : std::false_type {};

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

    template<uint64_t TARGET_ID> class target_t {
    public:
      static_assert(containsId(OD.TLS, TARGET_ID));
      static constexpr size_t TARGET_IDX = findId(OD.TLS, TARGET_ID);
      static constexpr targetLayout TL = OD.TLS[TARGET_IDX];
      static constexpr size_t maxFrameswap = frameswapLCM(TL.resources);
      static constexpr auto allObjectResourceSlots = getResourceSlots<TL.objectLayoutId>();

    private:
      onion<OD>* o;
      mappedResourceTuple<allObjectResourceSlots>* allObjectResources;
      std::map<uint64_t, frameBufferBundle[maxFrameswap]> fbByRpIdByFrame;
      std::map<uint64_t, descriptorUpdateData_t<allObjectResourceSlots.len>[maxFrameswap]> perShaderByIdByFrame;
      uint64_t objectId;

      target_t(const target_t&) = delete;
      target_t() = delete;
      explicit target_t(onion<OD>* o): o(o) {};

      friend onion;

    public:

      template<uint64_t resourceSlotId> auto& get() {
	static_assert(findResourceReferenceToSlot(TL.resources, resourceSlotId));//sanity check that this layout uses that slot
	return allObjectResources->template at<resourceSlotId>();
      };

      template<uint64_t resourceSlotId> void write(auto t, size_t hostAccessOffset = 0) {
	scopeLock lock(&o->mutex);
	get<resourceSlotId>().set(o->frame + hostAccessOffset, t);
      };

      template<uint64_t resourceSlotId> void set(auto* t) {
	static_assert(findResourceReferenceToSlot(TL.resources, resourceSlotId));//sanity check that this layout uses that slot
	scopeLock lock(&o->mutex);
	allObjectResources->template set<resourceSlotId>(t);
      };

    };

    template<size_t IDX = 0> struct targetCollectionTuple {
      iterableRecyclingPool<target_t<OD.TLS[IDX].id>, onion<OD>*> data;
      targetCollectionTuple<IDX+1> rest;

      targetCollectionTuple(onion<OD>* o) : data(o), rest(o) {};

      template<size_t TID, uint64_t TIDX = findId(OD.TLS, TID)> auto& ofLayout() {
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
      return allTargets.template ofLayout<targetId>().allocate();
    };

    template<uint64_t targetId> void freeTarget(target_t<targetId>* d) {
      allTargets.template ofLayout<targetId>().free(d);
    };

    template<uint64_t SOURCE_ID> class source_t {
    public:
      static_assert(containsId(OD.SLS, SOURCE_ID));
      static constexpr size_t SOURCE_IDX = findId(OD.SLS, SOURCE_ID);
      static constexpr sourceLayout SL = OD.SLS[SOURCE_IDX];
      static constexpr size_t maxFrameswap = frameswapLCM(SL.resources);
      static constexpr auto allObjectResourceSlots = getResourceSlots<SL.objectLayoutId>();

    private:
      onion<OD>* o;
      mappedResourceTuple<allObjectResourceSlots>* allObjectResources;
      std::map<uint64_t, descriptorUpdateData_t<allObjectResourceSlots.len>[maxFrameswap]> perShaderByIdByFrame;
      uint64_t objectId;

      source_t(const source_t&) = delete;
      source_t() = delete;
      explicit source_t(onion<OD>* o): o(o) {};

      friend onion;

    public:

      template<uint64_t resourceSlotId> auto& get() {
	static_assert(findResourceReferenceToSlot(SL.resources, resourceSlotId));//sanity check that this layout uses that slot
	return allObjectResources->template at<resourceSlotId>();
      };

      template<uint64_t resourceSlotId> void write(auto t, size_t hostAccessOffset = 0) {
	scopeLock lock(&o->mutex);
	get<resourceSlotId>().set(o->frame + hostAccessOffset, t);
      };

      template<uint64_t resourceSlotId> void set(auto* t) {
	static_assert(findResourceReferenceToSlot(SL.resources, resourceSlotId));//sanity check that this layout uses that slot
	scopeLock lock(&o->mutex);
	allObjectResources->template set<resourceSlotId>(t);
      };

    };

    template<size_t IDX = 0> struct sourceCollectionTuple {
      iterableRecyclingPool<source_t<OD.SLS[IDX].id>, onion<OD>*> data;
      sourceCollectionTuple<IDX+1> rest;

      sourceCollectionTuple(onion<OD>* o) : data(o), rest(o) {};

      template<size_t TID, uint64_t TIDX = findId(OD.SLS, TID)> auto& ofLayout() {
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
      return allSources.template ofLayout<sourceId>().allocate();
    };

    template<uint64_t sourceId> void freeSource(source_t<sourceId>* d) {
      allSources.template ofLayout<sourceId>().free(d);
    };

    template<literalList<sourceLayout> SLS> struct sourcePointerTuple {
      static constexpr uint64_t SID = SLS[0].id;
      typedef source_t<SID> T;
      T* data;
      sourcePointerTuple<SLS.sub(1)> rest;

      void createAll(onion<OD>* o, mappedResourceTuple<T::allObjectResourceSlots>* resources, uint64_t objectId) {
	data = o->createSource<SID>();
	data->allObjectResources = resources;
	data->objectId = objectId;
	if constexpr(SLS.len > 1)
	  rest.createAll(o, resources);
      };

      void freeAll(onion<OD>* o) {
	data = o->freeSource<SID>();
	if constexpr(SLS.len > 1)
	  rest.freeAll(o);
      };

      template<uint64_t ID> auto* get() {
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

      void createAll(onion<OD>* o, mappedResourceTuple<T::allObjectResourceslots>* resources, uint64_t objectId) {
	data = o->createTarget<TID>();
	data->allObjectResources = resources;
	data->objectId = objectId;
	if constexpr(TLS.len > 1)
	  rest.createAll(o, resources);
      };

      void freeAll(onion<OD>* o) {
	data = o->freeTarget<TID>();
	if constexpr(TLS.len > 1)
	  rest.freeAll(o);
      };

      template<uint64_t ID> auto* get() {
	if constexpr(TID == ID)
	  return data;
	else
	  return rest.template get<ID>();
      };

    };

    template<literalList<targetLayout> SLS> requires(SLS.len == 0) struct targetPointerTuple<SLS> : std::false_type {};

    template<uint64_t objectLayoutId>
    struct object_t {
      static constexpr auto RSS = getResourceSlots<objectLayoutId>();
      static constexpr auto SLS = getSourceLayouts<objectLayoutId>();
      static constexpr auto TLS = getTargetLayouts<objectLayoutId>();
      static constexpr auto allRRS = getResourceReferences<objectLayoutId>();
      static constexpr objectLayout OL = findById(OD.OLS, objectLayoutId);
      static constexpr bool hasWindow = OL.windowConsumerId != NONE;
      static constexpr resourceConsumer windowRC = consumerForWindowForObject(objectLayoutId);
      static constexpr resourceReference windowRR = findResourceReferenceToConsumer(allRRS, windowRC.id);
      static constexpr resourceSlot windowRS = findById(RSS, windowRR.resourceSlotId);

      uint64_t objectId;
      mappedResourceTuple<RSS> resources;
      targetPointerTuple<TLS> targets;
      sourcePointerTuple<SLS> sources;
      std::conditional_t<hasWindow, window, size_t> presentWindow = { OD.GPUID };
      onion<OD>* owner;

      explicit object_t(onion<OD>* o) : owner(o) {};

      inline auto& getWindow() {
	return presentWindow;
      };

      template<uint64_t resourceSlotId> auto& get() {
	return resources.template at<resourceSlotId>();
      };

      template<uint64_t resourceSlotId> void write(auto t, size_t hostAccessOffset = 0) {
	scopeLock lock(&owner->mutex);
	get<resourceSlotId>().set(owner->frame + hostAccessOffset, t);
      };

      template<uint64_t resourceSlotId> void set(auto* t) {
	scopeLock lock(&owner->mutex);
	resources.template set<resourceSlotId>(t);
      };

      inline void preRender(vk::CommandBuffer cmd) {
	if constexpr(hasWindow) {
	  auto windowExt = presentWindow.getSize3D();
	  resources.trackWindowSize(owner->frame, windowExt);
	  presentWindow.acquire();
	}
	resources.preRender(owner->frame, cmd, owner->getActiveGarbageCollector());
      };

      inline void postRender(vk::SemaphoreSubmitInfo& renderWaitSem) {
	if constexpr(hasWindow) {
	  auto& img = get<windowRS.id>();
	  static constexpr vk::ImageLayout layout = imageLayoutFor(windowRC.access);
	  static_assert(layout == vk::ImageLayout::eGeneral || layout == vk::ImageLayout::eTransferSrcOptimal);
	  auto f = owner->frame + windowRR.frameLatency;
	  presentWindow.present(img.frameImage(f), layout, img.getSizeOffset(f), renderWaitSem);
	}
      };

      void reinit(uint64_t frame, vk::CommandBuffer cmd) {
	resources.init(frame, cmd);
	objectId = owner->od.objectIdSeed++;
	targets.createAll(owner, &resources, objectId);
	sources.createAll(owner, &resources, objectId);
      };

      void free() {
	targets.freeAll(owner);
	sources.freeAll(owner);
      };

    };

    template<size_t IDX = 0> struct objectCollectionTuple {
      iterableRecyclingPool<object_t<OD.OLS[IDX].id>, onion<OD>*> data;
      objectCollectionTuple<IDX+1> rest;

      objectCollectionTuple(onion<OD>* o) : data(o), rest(o) {};

      template<size_t OID, uint64_t OIDX = findId(OD.OLS, OID)> auto& ofLayout() {
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
      scopeLock lock(&mutex);
      target_t<objectLayoutId>* ret = allObjects.template ofLayout<objectLayoutId>().allocate();
      auto cmd = dev->getTempCmd();
      ret->reinit(frame, cmd.cmd);
      cmd.submit();
      cmd.waitFor();
      return ret;
    };

    template<uint64_t objectLayoutId> void destroy(object_t<objectLayoutId>* d) {
      scopeLock lock(&mutex);
      d->free();
      allObjects.template ofLayout<objectLayoutId>().free(d);
    };

    template<literalList<resourceSlot> RSS,
	     literalList<resourceReference> RRS,
	     literalList<resourceConsumer> RCS, uint64_t RCIDX = 0>
    inline void fillWrites(descriptorUpdateData_t<RCS.len>& data, mappedResourceTuple<RSS>& rm, vk::DescriptorSet ds,
			   uint64_t frameMod, uint64_t frameLastUpdated) {
      if constexpr(RCIDX < RCS.len) {
	static constexpr resourceConsumer RC = RCS[RCIDX];
	static constexpr resourceReference RR = *findResourceReferenceToConsumer(RRS, RC.id);
	static constexpr resourceSlot RS = findById(RSS, RR.resourceSlotId);
	if constexpr(RC.usage.type == resourceUsageType::eDescriptor) {
	  auto& res = rm.template at<RS.id>();
	  if(frameLastUpdated == NONE || frameLastUpdated < res.frameUpdated(frameMod)) {
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
	      img.imageView = res.template createView<RR.subresource.image.viewType,
						      getSubresource(RS.requirementId, RR)>(frameMod);
	      img.imageLayout = imageLayoutFor(RC.access);
	    } else {
	      w.pImageInfo = NULL;
	      auto& buf = data.buffers[w.dstBinding];
	      w.pBufferInfo = &buf;
	      buf.buffer = res.frameBuffer(frameMod);
	      buf.offset = RR.subresource.bufferRange.offset;
	      buf.range = RR.subresource.bufferRange.length ? RR.subresource.bufferRange.length : VK_WHOLE_SIZE;
	      // WARN("wrote buffer descriptor ", buf.buffer, " to binding ", w.dstBinding, " on set ", ds);
	    }
	    data.writeCount++;
	  } else {
	    data.skipCount++;
	  }
	}
	fillWrites<RRS, RCS, RCIDX+1>(data, rm, ds, frameMod, frameLastUpdated);
      }
    };

    template<literalList<resourceSlot> RSS,
	     literalList<resourceReference> RRS,
	     literalList<resourceConsumer> RCS>
    inline void prepareDescriptors(descriptorUpdateData_t<RSS.len>& descriptorBundle,
				   std::unique_ptr<descriptorPoolPoolBase>& dpp,
				   mappedResourceTuple<RSS>& resources,
				   size_t frameMod) {
      if(!descriptorBundle.descriptorSet) [[unlikely]] {
	if(!dpp) [[unlikely]]
	  dpp.reset(new descriptorPoolPool<RCS, OD.GPUID>());
	descriptorBundle.descriptorSet = dpp->allocate();
	descriptorBundle.reset();
	fillWrites<RSS, RRS, RCS>(descriptorBundle, resources, descriptorBundle.descriptorSet, frameMod, NONE);
	dev->getVkDevice().updateDescriptorSets(descriptorBundle.writeCount, descriptorBundle.writes, 0, NULL);
	descriptorBundle.frameLastUpdated = frame;
      } else if(descriptorBundle.frameLastUpdated < resources.frameLastUpdated(frame)) [[unlikely]] {
	descriptorBundle.reset();
	fillWrites<RSS, RRS, RCS>(descriptorBundle, resources, descriptorBundle.descriptorSet, frameMod, descriptorBundle.frameLastUpdated);
	dev->getVkDevice().updateDescriptorSets(descriptorBundle.writeCount, descriptorBundle.writes, 0, NULL);
	descriptorBundle.frameLastUpdated = frame;
      }
    };

    static consteval bool satisfies(literalList<resourceReference> resources, literalList<resourceConsumer> references) {
      for(resourceConsumer rc : references)
	if(!findResourceReferenceToConsumer(resources, rc.id))
	  return false;
      return true;
    };

    static consteval std::vector<resourceBarrier> allBarriers() {
      std::vector<resourceBarrier> allBarriers;
      for(const resourceSlot& RS : OD.RSS)
	concat(allBarriers, findBarriers(RS.id));
      return allBarriers;
    };

    static consteval std::vector<resourceBarrier> barriersForTime_v(resourceBarrierTiming BT) {
      auto ret = allBarriers();
      std::erase_if(ret, [BT](auto x){ return x.timing != BT; });
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

    template<literalList<resourceBarrier> RBS> inline void recordBarriers(vk::CommandBuffer cmd) {
      if constexpr(RBS.len) {
	constexpr size_t barrierBatchSize = 256;
	constexpr resourceBarrier RB = RBS[0];
	constexpr bool isImage = containsId(OD.IRS, RB.requirementId);
	vk::DependencyInfo DI;
	if constexpr(isImage) {
	  //NOTE: This function is under the onion mutex.
	  //the only field that ever changes is the image handle itself, so save this for future reuse.
	  static copyableArray<vk::ImageMemoryBarrier2, barrierBatchSize> barriers(vk::ImageMemoryBarrier2 {
	      toPipelineStage2(RB.before),
	      RB.before.usage.access,
	      toPipelineStage2(RB.after),
	      RB.after.usage.access,
	      imageLayoutFor(RB.before.usage.access),
	      imageLayoutFor(RB.after.usage.access),
	      {}, {}, {},
	      getAllInclusiveSubresource(findById(OD.IRS, RB.requirementId))
	    });
	  DI.pImageMemoryBarriers = barriers.ptr();
	  DI.imageMemoryBarrierCount = barrierBatchSize;
	  size_t mbIdx = 0;
	  for(object_t<RB.objectLayoutId>* cluster : allObjects.template ofLayout<RB.objectLayoutId>()) {
	    auto& img = cluster->template get<RB.resourceSlotId>();
	    barriers[mbIdx].image = img.frameImage(RB.frameLatency + frame);
	    WITE_DEBUG_IB(barriers[mbIdx], cmd);
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
	      RB.before.usage.access,
	      toPipelineStage2(RB.after),
	      RB.after.usage.access,
	      {}, {}, {}, 0, VK_WHOLE_SIZE
	    });
	  DI.pBufferMemoryBarriers = barriers.ptr();
	  DI.bufferMemoryBarrierCount = barrierBatchSize;
	  size_t mbIdx = 0;
	  for(object_t<RB.objectLayoutId>* cluster : allObjects.template ofLayout<RB.objectLayoutId>()) {
	    barriers[mbIdx].buffer = cluster->template get<RB.resourceSlotId>().frameBuffer(RB.frameLatency + frame);
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
	recordBarriers<RBS.sub(1)>(cmd);
      }
    };

    template<resourceBarrierTiming BT> inline void recordBarriersForTime(vk::CommandBuffer cmd) {
      static constexpr auto BTS = barriersForTime<BT>();
      recordBarriers<BTS>(cmd);
    };

    template<copyStep CS, literalList<uint64_t> XLS> inline void recordCopies(vk::CommandBuffer cmd) {
      if constexpr(XLS.len) {
	static constexpr auto XL = findLayoutById<XLS[0]>();//could be targetLayout or sourceLayout
	static constexpr const resourceReference *SRR = findResourceReferenceToConsumer(XL.resources, CS.src),
	  *DRR = findResourceReferencing(XL.resources, CS.dst);
	if constexpr(SRR && DRR) {//skip copy if they're not both represented
	  static constexpr resourceSlot SRS = findById(OD.RSS, SRR->resourceSlotId),
	    DRS = findById(OD.RSS, DRR->resourceSlotId);
	  static constexpr bool srcIsImage = containsId(OD.IRS, SRS.requirementId),
	    dstIsImage = containsId(OD.IRS, DRS.requirementId);
	  static_assert(!(srcIsImage ^ dstIsImage));//NYI buffer to/from image (why would you do that?)
	  if constexpr(srcIsImage && dstIsImage) {
	    //always blit, no way to know at compile time if they're the same size
	    //TODO resolve if different sample counts
	    std::array<vk::Offset3D, 2> srcBounds, dstBounds;
	    vk::ImageBlit blitInfo(getAllInclusiveSubresourceLayers(findById(OD.IRS, SRS.requirementId)), srcBounds,
				   getAllInclusiveSubresourceLayers(findById(OD.IRS, DRS.requirementId)), dstBounds);
	    for(auto* cluster : getAllOfLayout<XL.id>()) {
	      auto& src = cluster->template get<SRS.id>();
	      auto& dst = cluster->template get<DRS.id>();
	      blitInfo.srcOffsets = src.getSize(SRR->frameLatency + frame);
	      blitInfo.dstOffsets = dst.getSize(DRR->frameLatency + frame);
	      cmd.blitImage(src.frameImage(SRR->frameLatency + frame),
			    imageLayoutFor(vk::AccessFlagBits2::eTransferRead),
			    dst.frameImage(DRR->frameLatency + frame),
			    imageLayoutFor(vk::AccessFlagBits2::eTransferWrite),
			    1, &blitInfo, CS.filter);
	    }
	  } else {//buffer to buffer
	    static constexpr bufferRequirements SBR = findById(OD.BRS, SRS.requirementId),
	      TBR = findById(OD.BRS, DRS.requirementId);
	    static constexpr vk::BufferCopy copyInfo(0, 0, min(SBR.size, TBR.size));
	    for(auto* cluster : getAllOfLayout<XL.id>()) {
	      auto& src = cluster->template get<SRS.id>();
	      auto& dst = cluster->template get<DRS.id>();
	      cmd.copyBuffer(src.frameBuffer(SRR->frameLatency + frame),
			     dst.frameBuffer(DRR->frameLatency + frame),
			     1, &copyInfo);
	      // WARN("copied buffer ", src.frameBuffer(CS.src.frameLatency + frame), " to ", dst.frameBuffer(CS.dst.frameLatency + frame));
	    }
	  }
	}
	recordCopies<CS, XLS.sub(1)>(cmd);
      }
    };

    template<literalList<uint64_t> CSIDS, layerRequirements LR> inline void recordCopies(vk::CommandBuffer cmd) {
      if constexpr(CSIDS.len) {
	static constexpr copyStep CS = findById(OD.CSS, CSIDS[0]);
	recordCopies<CS, allLayoutIds>(cmd);
	recordCopies<CSIDS.sub(1), LR>(cmd);
      }
    };

    template<clearStep CS, literalList<uint64_t> XLS> inline void recordClears(vk::CommandBuffer cmd) {
      if constexpr(XLS.len) {
	static constexpr auto XL = findLayoutById<XLS[0]>();//source or target layout
	static constexpr const resourceReference* RR = findResourceReferenceToConsumer(XL.resources, CS.id);
	if constexpr(RR) {
	  static constexpr resourceSlot RS = findById(OD.RSS, RR->resourceSlotId);
	  static_assert(containsId(OD.IRS, RS.requirementId));
	  static constexpr imageRequirements IR = findById(OD.IRS, RS.requirementId);
	  static constexpr vk::ImageSubresourceRange SR = getAllInclusiveSubresource(IR);
	  for(auto& cluster : getAllOfLayout<XL.id>()) {
	    auto img = cluster->template get<RS.id>().frameImage(RR->frameLatency + frame);
	    if constexpr(isDepth(IR)) {
	      cmd.clearDepthStencilImage(img, vk::ImageLayout::eTransferDstOptimal, &CS.clearValue.depthStencil, 1, &SR);
	    } else {
	      cmd.clearColorImage(img, vk::ImageLayout::eTransferDstOptimal, &CS.clearValue.color, 1, &SR);
	    }
	  }
	}
	recordClears<CS, XLS.sub(1)>(cmd);
      }
    };

    template<literalList<uint64_t> CLIDS, layerRequirements LR> inline void recordClears(vk::CommandBuffer cmd) {
      if constexpr(CLIDS.len) {
	static constexpr clearStep CS = findById(OD.CLS, CLIDS[0]);
	recordClears<CS, allLayoutIds>(cmd);
	recordClears<CLIDS.sub(1), LR>(cmd);
      }
    };

    template<renderPassRequirements, targetLayout> struct renderPassInfoBundle : std::false_type {};

    template<renderPassRequirements RP, targetLayout TL> requires(RP.depth != NONE) struct renderPassInfoBundle<RP, TL> {
      static constexpr resourceConsumer colorRC = consumerForColorAttachment(RP.color),
	depthRC = consumerForDepthAttachment(RP.depth);
      static constexpr const resourceReference colorRR = *findResourceReferenceToConsumer(TL.resources, colorRC.id),
	depthRR = *findResourceReferenceToConsumer(TL.resources, colorRC.id);
      static constexpr const resourceSlot colorRS = findById(OD.RSS, colorRR.resourceSlotId),
	depthRS = findById(OD.RSS, depthRR.resourceSlotId);
      static constexpr imageRequirements colorIR = findById(OD.IRS, colorRS.requirementId),
	depthIR = findById(OD.IRS, depthRS.requirementId);
      static constexpr vk::AttachmentReference colorRef { 0, imageLayoutFor(colorRC.access) },
	depthRef { 1, imageLayoutFor(depthRC.access) };
      static constexpr vk::SubpassDescription subpass { {}, vk::PipelineBindPoint::eGraphics, 0, NULL, 1, &colorRef, NULL, &depthRef };
      static constexpr vk::AttachmentDescription attachments[2] = {//MAYBE variable attachments when inputs are allowed
	{ {}, colorIR.format, vk::SampleCountFlagBits::e1, RP.clearColor ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, imageLayoutFor(colorRC.access), colorRef.layout },
	{ {}, depthIR.format, vk::SampleCountFlagBits::e1, RP.clearDepth ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, imageLayoutFor(depthRC.access), depthRef.layout }
      };
      //TODO multiview
      //static constexpr vk::RenderPassMultiviewCreateInfo multiview {
      static constexpr vk::RenderPassCreateInfo rpci { {}, 2, attachments, 1, &subpass, 0 };
    };

    template<renderPassRequirements RP, targetLayout TL> requires(RP.depth == NONE) struct renderPassInfoBundle<RP, TL> {
      static constexpr resourceConsumer colorRC = consumerForColorAttachment(RP.color);
      static constexpr const resourceReference colorRR = *findResourceReferenceToConsumer(TL.resources, colorRC.id);
      static constexpr const resourceSlot colorRS = findById(OD.RSS, colorRR.resourceSlotId);
      static constexpr imageRequirements colorIR = findById(OD.IRS, colorRS.requirementId);
      static constexpr vk::AttachmentReference colorRef { 0, imageLayoutFor(colorRC.access) };
      static constexpr vk::SubpassDescription subpass { {}, vk::PipelineBindPoint::eGraphics, 0, NULL, 1, &colorRef, NULL, NULL };
      static constexpr vk::AttachmentDescription attachments[1] = {//MAYBE variable attachments when inputs are allowed
	{ {}, colorIR.format, vk::SampleCountFlagBits::e1, RP.clearColor ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, imageLayoutFor(colorRC.access), colorRef.layout }
      };
      //TODO multiview
      //static constexpr vk::RenderPassMultiviewCreateInfo multiview {
      static constexpr vk::RenderPassCreateInfo rpci { {}, 1, attachments, 1, &subpass, 0 };
    };

    //MAYBE match like clusters of shaders, sources, and targets so they can share layouts and descriptor pools?

    template<targetLayout TL, renderPassRequirements RP, graphicsShaderRequirements GSR, literalList<sourceLayout> SLS>
    inline void recordRenders(auto& target, perTargetLayout& ptl, perTargetLayoutPerShader& ptlps, descriptorUpdateData_t<GSR.targetProvidedResources.len>& targetDescriptors, vk::RenderPass rp, vk::CommandBuffer cmd) {
      if constexpr(SLS.len) {
	static constexpr sourceLayout SL = SLS[0];
	// WARN("  using source ", SL.id);
	if constexpr(satisfies(SL.resources, GSR.sourceProvidedResources)) {
	  //for now (at least), only one vertex binding of each input rate type. Source only.
	  struct findVB {
	    consteval bool operator()(const resourceConsumer& rr) {
	      return rr.usage.type == resourceUsageType::eVertex &&
		rr.usage.asVertex.rate == vk::VertexInputRate::eVertex &&
		findResourceReferenceToConsumer(SL.resources, rr.id);
	    };
	  };
	  struct findIB {
	    consteval bool operator()(const resourceConsumer& rr) {
	      return rr.usage.type == resourceUsageType::eVertex &&
		rr.usage.asVertex.rate == vk::VertexInputRate::eInstance &&
		findResourceReferenceToConsumer(SL.resources, rr.id);
	    };
	  };
	  static_assert(GSR.sourceProvidedResources.countWhereCE(findVB()) <= 1);
	  static_assert(GSR.sourceProvidedResources.countWhereCE(findIB()) <= 1);
	  static constexpr const resourceConsumer* vb = GSR.sourceProvidedResources.firstWhere(findVB());
	  static constexpr const resourceConsumer* ib = GSR.sourceProvidedResources.firstWhere(findIB());
	  static constexpr size_t vibCount = (ib ? 1 : 0) + (vb ? 1 : 0);
	  shaderInstance& shaderInstance = ptlps.perSL[SL.id];
	  perSourceLayoutPerShader& pslps = od.perSL[SL.id].perShader[GSR.id];
	  if(!pslps.descriptorPool) [[unlikely]]
	    pslps.descriptorPool.reset(new descriptorPoolPool<GSR.sourceProvidedResources, OD.GPUID>());
	  if(!shaderInstance.pipeline) [[unlikely]] {
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
	    static constexpr vk::PipelineDepthStencilStateCreateInfo depth = { {}, RP.depth != NONE, true, vk::CompareOp::eLessOrEqual };//not set: depth bounds and stencil test stuff
	    static constexpr vk::PipelineColorBlendStateCreateInfo blend = { {}, false, vk::LogicOp::eNoOp, 1, &GSR.blend, { 1, 1, 1, 1 } };
	    static constexpr vk::DynamicState dynamics[] = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
	    static constexpr vk::PipelineDynamicStateCreateInfo dynamic = { {}, 2, dynamics };
	    vk::GraphicsPipelineCreateInfo gpci { {}, GSR.modules.len, stages, &verts, &assembly, /*tesselation*/ NULL, &vp, &raster, &multisample, &depth, &blend, &dynamic, shaderInstance.pipelineLayout, rp, /*subpass idx*/ 0 };//not set: derivitive pipeline stuff
	    VK_ASSERT(dev->getVkDevice().createGraphicsPipelines(dev->getPipelineCache(), 1, &gpci, ALLOCCB, &shaderInstance.pipeline), "Failed to create graphics pipeline ", GSR.id);
	  }
	  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderInstance.pipeline);
	  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 1, 1, &targetDescriptors.descriptorSet, 0, NULL);
	  for(source_t<SL.id>* source : allSources.template ofLayout<SL.id>()) {
	    if constexpr(!TL.selfRender && SL.objectLayoutId == TL.objectLayoutId)
	      if(source->objectId == target.objectId) [[unlikely]]
		continue;
	    size_t frameMod = frame % source_t<SL.id>::maxFrameswap;
	    auto& descriptorBundle = source->perShaderByIdByFrame[GSR.id][frameMod];
	    prepareDescriptors<object_t<SL.objectLayoutId>::RSS, SL.resources, GSR.sourceProvidedResources>
	      (descriptorBundle, pslps.descriptorPool, *source->allObjectResources, frameMod);
	    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	    //for now, source must provide all vertex info
	    vk::Buffer verts[vibCount];
	    vk::DeviceSize offsets[2];
	    static_assert(vb || GSR.vertexCountOverride);//vertex data required if vertex count not given statically
	    vk::DeviceSize vertices, instances;
	    if constexpr(vb) {
	      static constexpr resourceReference vbm = *findResourceReferenceToConsumer(SL.resources, vb->id); //compiler-time error: dereferencing null if the source did not provide a reference to the vertex buffer used by the shader
	      static constexpr resourceSlot vbs = findById(OD.RSS, vbm.resourceSlotId);
	      verts[0] = source->template get<vbm.resourceSlotId>().frameBuffer(frame + vbm.frameLatency);
	      offsets[0] = vbm.subresource.bufferRange.offset;
	      vertices = GSR.vertexCountOverride ? GSR.vertexCountOverride :
		vbm.subresource.bufferRange.length ? vbm.subresource.bufferRange.length :
		findById(OD.BRS, vbs.requirementId).size / sizeofUdm<vb->usage.asVertex.format>();
	    } else {
	      vertices = GSR.vertexCountOverride;
	    }
	    if constexpr(ib) {
	      static constexpr resourceReference ibm = *findResourceReferenceToConsumer(SL.resources, ib->id); //compiler-time error: dereferencing null if the source did not provide a reference to the instance buffer used by the shader
	      static constexpr resourceSlot ibs = findById(OD.RSS, ibm.resourceSlotId);
	      verts[vibCount-1] = source->template get<ibm.resourceSlotId>().frameBuffer(frame + ibm.frameLatency);
	      offsets[vibCount-1] = ibm.subresource.bufferRange.offset;
	      instances = GSR.instanceCountOverride ? GSR.instanceCountOverride :
		ibm.subresource.bufferRange.length ? ibm.subresource.bufferRange.length :
		findById(OD.BRS, ibs.requirementId).size / sizeofUdm<ib->usage.asVertex.format>();
	    } else {
	      //but instances defaults to 1
	      instances = GSR.instanceCountOverride ? GSR.instanceCountOverride : 1;
	    }
	    if constexpr(vibCount) {
	      cmd.bindVertexBuffers(0, vibCount, verts, offsets);
	    }
	    cmd.draw(vertices, instances, 0, 0);
	    // WARN("Drew ", vertices, " from nested target-source");
	    //TODO more flexibility with draw. Allow source layout to ask for multi-draw, indexed, indirect etc. Allow (dynamic) less than the whole buffer.
	  }
	}
	recordRenders<TL, RP, GSR, SLS.sub(1)>(target, ptl, ptlps, targetDescriptors, rp, cmd);
      }
    };

    template<renderPassRequirements RP, graphicsShaderRequirements GSR>
    inline void recordRenders_targetOnly(auto& target, perTargetLayout& ptl, perTargetLayoutPerShader& ptlps, descriptorUpdateData_t<GSR.targetProvidedResources.len>& targetDescriptors, vk::RenderPass rp, vk::CommandBuffer cmd) {
      //for now, target only rendering must not supply a vertex or instance buffer, so vert count must come from an override
      shaderInstance& shaderInstance = ptlps.targetOnlyShader;
      if(!shaderInstance.pipeline) [[unlikely]] {
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
	static constexpr vk::PipelineDepthStencilStateCreateInfo depth = { {}, RP.depth != NONE, true, vk::CompareOp::eLessOrEqual };//not set: depth bounds and stencil test stuff
	static constexpr vk::PipelineColorBlendStateCreateInfo blend = { {}, false, vk::LogicOp::eNoOp, 1, &GSR.blend, { 1, 1, 1, 1 } };
	static constexpr vk::DynamicState dynamics[] = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
	static constexpr vk::PipelineDynamicStateCreateInfo dynamic = { {}, 2, dynamics };
	vk::GraphicsPipelineCreateInfo gpci { {}, GSR.modules.len, stages, &verts, &assembly, /*tesselation*/ NULL, &vp, &raster, &multisample, &depth, &blend, &dynamic, shaderInstance.pipelineLayout, rp, /*subpass idx*/ 0 };//not set: derivitive pipeline stuff
	VK_ASSERT(dev->getVkDevice().createGraphicsPipelines(dev->getPipelineCache(), 1, &gpci, ALLOCCB, &shaderInstance.pipeline), "Failed to create graphics pipeline ", GSR.id);
      }
      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderInstance.pipeline);
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 0, 1, &targetDescriptors.descriptorSet, 0, NULL);
      static_assert(GSR.vertexCountOverride > 0);
      cmd.draw(GSR.vertexCountOverride, GSR.instanceCountOverride ? GSR.instanceCountOverride : 1, 0, 0);
      // WARN("Drew ", GSR.vertexCountOverride, " from target only");
      //TODO more flexibility with draw. Allow source layout to ask for multi-draw, indexed, indirect etc. Allow (dynamic) less than the whole buffer.
    };

    template<size_t layerIdx, targetLayout TL, renderPassRequirements RP, literalList<graphicsShaderRequirements> GSRS>
    inline void recordRenders(auto& target, perTargetLayout& ptl, vk::RenderPass rp, vk::CommandBuffer cmd) {
      if constexpr(GSRS.len) {
	static constexpr graphicsShaderRequirements GSR = GSRS[0];
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::render, .passId = RP.id, .shaderId = GSR.id }>(cmd);
	if constexpr(satisfies(TL.resources, GSR.targetProvidedResources)) {
	  // WARN("Begin shader ", GSR.id, " using target ", TL.id);
	  size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	  auto& descriptorBundle = target.perShaderByIdByFrame[GSR.id][frameMod];
	  perTargetLayoutPerShader& ptlps = ptl.perShader[GSR.id];
	  prepareDescriptors<object_t<TL.objectLayoutId>::RSS, TL.resources, GSR.targetProvidedResources>
	    (descriptorBundle, ptlps.descriptorPool, *target.allObjectResources, frameMod);
	  if constexpr(GSR.sourceProvidedResources.len)
	    recordRenders<TL, RP, GSR, OD.SLS>(target, ptl, ptlps, descriptorBundle, rp, cmd);
	  else
	    recordRenders_targetOnly<RP, GSR>(target, ptl, ptlps, descriptorBundle, rp, cmd);
	}
	recordRenders<layerIdx, TL, RP, GSRS.sub(1)>(target, ptl, rp, cmd);
      }
    };

    template<size_t layerIdx, targetLayout TL, literalList<uint64_t> RPIDS>
    inline void recordRenders(auto& target, perTargetLayout& ptl, vk::CommandBuffer cmd) {
      if constexpr(RPIDS.len) {
	static constexpr renderPassRequirements RP = findById(OD.RPRS, RPIDS[0]);
	static constexpr const resourceReference* colorRR = findResourceReferenceToConsumer(TL.resources, RP.color),
	  *depthRR = findResourceReferenceToConsumer(TL.resources, RP.depth);
	if constexpr(colorRR != NULL && (depthRR != NULL || RP.depth == NONE)) {
	  vk::RenderPass& rp = ptl.rpByRequirementId[RP.id];
	  if(!rp) [[unlikely]] {
	    VK_ASSERT(dev->getVkDevice().createRenderPass(&renderPassInfoBundle<RP, TL>::rpci, ALLOCCB, &rp),
		      "failed to create render pass ", TL.id, " ", RP.id);
	  }
	  auto& color = target.template get<colorRR->resourceSlotId>();
	  static constexpr resourceSlot colorRS = findById(OD.RSS, colorRR->resourceSlotId);
	  frameBufferBundle& fbb = target.fbByRpIdByFrame[RP.id][frame % target_t<TL.id>::maxFrameswap];
	  bool depthOutdated;
	  if constexpr(RP.depth != NONE)
	    depthOutdated = fbb.fb && fbb.lastUpdatedFrame < target.template get<depthRR->resourceSlotId>().frameUpdated(frame + depthRR->frameLatency);
	  else
	    depthOutdated = false;
	  bool colorOutdated = fbb.fb && fbb.lastUpdatedFrame < color.frameUpdated(frame + colorRR->frameLatency);
	  vk::Rect2D size = color.getSizeRect(frame + colorRR->frameLatency);
	  if(!fbb.fb || colorOutdated || depthOutdated) [[unlikely]] {
	    if(fbb.fb)
	      getActiveGarbageCollector().push(fbb.fb);
	    if constexpr(RP.depth != NONE)
	      ASSERT_TRAP(size == target.template get<depthRR->resourceSlotId>().getSizeRect(frame + depthRR->frameLatency), "framebuffer depth color size mismatch");
	    fbb.fbci.renderPass = rp;
	    fbb.fbci.width = size.extent.width;
	    fbb.fbci.height = size.extent.height;
	    fbb.fbci.layers = 1;
	    if constexpr(RP.depth == NONE)
	      fbb.fbci.attachmentCount = 1;
	    else
	      fbb.fbci.attachmentCount = 2;
	    if(colorOutdated)
	      getActiveGarbageCollector().push(fbb.attachments[0]);
	    if(depthOutdated)
	      getActiveGarbageCollector().push(fbb.attachments[1]);
	    if(!fbb.fb || colorOutdated)
	      fbb.attachments[0] = color.template createView<colorRR->subresource.image.viewType,
							     getSubresource(colorRS.requirementId, *colorRR)>
		(frame + colorRR->frameLatency);
	    if constexpr(RP.depth != NONE)
	      if(!fbb.fb || depthOutdated)
		fbb.attachments[1] = target.template get<depthRR->resourceSlotId>().
		  template createView<depthRR->subresource.image.viewType,
				      getSubresource(findById(OD.RSS, depthRR->resourceSlotId).requirementId, *depthRR)>
		  (frame + depthRR->frameLatency);
	    VK_ASSERT(dev->getVkDevice().createFramebuffer(&fbb.fbci, ALLOCCB, &fbb.fb), "failed to create framebuffer");
	    fbb.lastUpdatedFrame = frame;
	  }
	  //MAYBE multiview
	  vk::Viewport viewport = { 0, 0, (float)size.extent.width, (float)size.extent.height, 0.0f, 1.0f };
	  cmd.setViewport(0, 1, &viewport);
	  cmd.setScissor(0, 1, &size);
	  static constexpr vk::ClearValue clears[2] { RP.clearColorValue, RP.clearDepthValue };
	  vk::RenderPassBeginInfo rpBegin(rp, fbb.fb, size, (uint32_t)RP.clearColor + (uint32_t)RP.clearDepth, RP.clearColor ? clears : clears+1);
	  recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::render, .passId = RP.id, .shaderId = NONE }>(cmd);
	  cmd.beginRenderPass(&rpBegin, vk::SubpassContents::eInline);
	  // WARN("RP begin");
	  recordRenders<layerIdx, TL, RP, RP.shaders>(target, ptl, rp, cmd);
	  cmd.endRenderPass();
	}
	recordRenders<layerIdx, TL, RPIDS.sub(1)>(target, ptl, cmd);
      }
    };

    template<size_t layerIdx, targetLayout TL, layerRequirements LR>
    inline void recordRenders(perTargetLayout& ptl, vk::CommandBuffer cmd) {
      //TODO spawn each of these in a parallel cmd; rework call stack signatures to hand semaphores around instead of cmd
      //^^  ONLY if there is no writable source descriptor
      for(auto* target : allTargets.template ofLayout<TL.id>()) {
	recordRenders<layerIdx, TL, LR.renders>(*target, ptl, cmd);
      }
    };

    template<size_t layerIdx, literalList<uint64_t> TLS, layerRequirements LR>
    inline void recordRenders(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	static constexpr targetLayout TL = findById(OD.TLS, TLS[0]);
	recordRenders<layerIdx, TL, LR>(od.perTL[TL.id], cmd);
	recordRenders<layerIdx, TLS.sub(1), LR>(cmd);
      };
    };

    // template<uint64_t RCID, uint64_t TLID, uint64_t SLID>
    // static consteval resourceReference resolveReference() {
    //   const resourceReference* RR;
    //   if constexpr(TLID != NONE)
    // 	RR = findResourceReferenceToConsumer(findById(OD.TLS, TLID).resources, RCID);
    //   if constexpr(SLID != NONE)
    // 	if(RR == NULL)
    // 	  RR = findResourceReferenceToConsumer(findById(OD.SLS, SLID).resources, RCID);
    //   return *RR;//error bc null if neither layout provides it
    // };

    template<computeShaderRequirements CS, uint64_t TID, uint64_t SID> static inline void getWorkgroupSize(vk::Extent3D& workgroupSize, target_t<TID>* target, source_t<SID>* source, uint64_t frameMod) {
      static constexpr resourceSlot RS = findById(OD.RSS, CS.primaryOutputSlotId);
      static constexpr targetLayout TL = TID != NONE ? findById(OD.TLS, TID) : targetLayout();
      static constexpr sourceLayout SL = SID != NONE ? findById(OD.SLS, TID) : sourceLayout();
      static constexpr const resourceReference* TRR = findResourceReferenceToConsumer(TL.resources, RS.id);
      static constexpr const resourceReference* SRR = findResourceReferenceToConsumer(SL.resources, RS.id);
      static_assert(TRR || SRR);
      if constexpr(containsId(OD.IRS, RS.requirementId)) {//is image
	vk::Extent3D imageSize;
	if constexpr(TRR)
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

    template<computeShaderRequirements CS, literalList<uint64_t> SLS>
    inline void recordComputeDispatches_sourceOnly(vk::CommandBuffer cmd) {
      asm("int3");//NYI
    };

    template<computeShaderRequirements CS, literalList<uint64_t> TLS>
    inline void recordComputeDispatches_targetOnly(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	static constexpr targetLayout TL = findById(OD.TLS, TLS[0]);
	if constexpr(satisfies(TL.resources, CS.targetProvidedResources)) {
	  for(target_t<TL.id>* target : allTargets.template ofLayout<TL.id>()) {
	    perTargetLayoutPerShader& ptlps = od.perTL[TL.id].perShader[CS.id];
	    size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	    auto& descriptorBundle = target->perShaderByIdByFrame[CS.id][frameMod];
	    prepareDescriptors<object_t<TL.objectLayoutId>::RSS, TL.resources, CS.targetProvidedResources>
	      (descriptorBundle, ptlps.descriptorPool, *target->allObjectResources, frameMod);
	    if(!ptlps.targetOnlyShader.pipeline) [[unlikely]] {
	      ptlps.targetOnlyShader.pipelineLayout = dev->getPipelineLayout<descriptorPoolPool<CS.targetProvidedResources, OD.GPUID>::dslci>();
	      static_assert(CS.module->stage == vk::ShaderStageFlagBits::eCompute);
	      vk::ComputePipelineCreateInfo ci;
	      shaderFactory<CS.module, OD.GPUID>::create(&ci.stage);
	      ci.layout = ptlps.targetOnlyShader.pipelineLayout;
	      VK_ASSERT(dev->getVkDevice().createComputePipelines(dev->getPipelineCache(), 1, &ci, ALLOCCB, &ptlps.targetOnlyShader.pipeline), "failed to create compute pipeline");
	    }
	    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ptlps.targetOnlyShader.pipeline);
	    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, ptlps.targetOnlyShader.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	    vk::Extent3D workgroupSize;
	    getWorkgroupSize<CS, TL.id, NONE>(workgroupSize, target, NULL, frameMod);
	    cmd.dispatch(workgroupSize.width, workgroupSize.height, workgroupSize.depth);
	  }
	}
	recordComputeDispatches_targetOnly<CS, TLS.sub(1)>(cmd);
      }
    };

    template<computeShaderRequirements CS, targetLayout TL, literalList<uint64_t> SLS>
    inline void recordComputeDispatches_nested(target_t<TL.id>* target, perTargetLayoutPerShader& ptlps, descriptorUpdateData_t<CS.targetProvidedResources.len>& perShader, vk::CommandBuffer cmd) {
      if constexpr(SLS.len) {
	static constexpr sourceLayout SL = findById(OD.SLS, SLS[0]);
	if constexpr(satisfies(SL.resources, CS.sourceProvidedResources)) {
	  shaderInstance& shaderInstance = ptlps.perSL[SL.id];
	  perSourceLayoutPerShader& pslps = od.perSL[SL.id].perShader[CS.id];
	  if(!pslps.descriptorPool) [[unlikely]]
	    pslps.descriptorPool.reset(new descriptorPoolPool<CS.sourceProvidedResources, OD.GPUID>());
	  if(!shaderInstance.pipeline) [[unlikely]] {
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
	  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shaderInstance.pipelineLayout, 1, 1, &perShader.descriptorSet, 0, NULL);
	  for(source_t<SL.id>* source : allSources.template ofLayout<SL.id>()) {
	    if constexpr(!TL.selfRender && SL.objectLayoutId == TL.objectLayoutId)
	      if(source->objectId == target->objectId) [[unlikely]]
		continue;
	    size_t frameMod = frame % source_t<SL.id>::maxFrameswap;
	    auto& descriptorBundle = source->perShaderByIdByFrame[CS.id][frameMod];
	    prepareDescriptors<object_t<SL.objectLayoutId>::RSS, SL.resources, CS.sourceProvidedResources>
	      (descriptorBundle, pslps.descriptorPool, source->resources, frameMod);
	    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shaderInstance.pipelineLayout, 0, 1, &descriptorBundle.descriptorSet, 0, NULL);
	    vk::Extent3D workgroupSize;
	    getWorkgroupSize<CS, TL.id, SL.id>(workgroupSize, target, source, frameMod);
	    cmd.dispatch(workgroupSize.width, workgroupSize.height, workgroupSize.depth);
	  }
	}
	recordComputeDispatches_nested<CS, TL, SLS.sub(1)>(target, ptlps, perShader, cmd);
      }
    };

    template<computeShaderRequirements CS, literalList<uint64_t> TLS, literalList<uint64_t> SLS>
    inline void recordComputeDispatches_nested(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	static constexpr targetLayout TL = findById(OD.TLS, TLS[0]);
	if constexpr(satisfies(TL.resources, CS.targetProvidedResources)) {
	  for(target_t<TL.id>* target : allTargets.template ofLayout<TL.id>()) {
	    perTargetLayoutPerShader& ptlps = od.perTL[TL.id].perShader[CS.id];
	    size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	    auto& descriptorBundle = target->perShaderByIdByFrame[CS.id][frameMod];
	    prepareDescriptors<object_t<TL.objectLayoutId>::RSS, TL.resources, CS.targetProvidedResources>
	      (descriptorBundle, ptlps.descriptorPool, target->resources, frameMod);
	    recordComputeDispatches_nested<CS, TL, SLS>(target, ptlps, descriptorBundle, cmd);
	  }
	}
	recordComputeDispatches_nested<CS, TLS.sub(1), SLS>(cmd);
      }
    };

    template<size_t layerIdx, layerRequirements LR, literalList<uint64_t> CSS>
    inline void recordComputeDispatches(vk::CommandBuffer cmd) {
      if constexpr(CSS.len) {
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::compute, .shaderId = CSS[0] }>(cmd);
	static constexpr computeShaderRequirements CS = findById(OD.CSRS, CSS[0]);
	if constexpr(CS.targetProvidedResources.len == 0)
	  recordComputeDispatches_sourceOnly<CS, OD.SLS>(cmd);
	else if constexpr(CS.sourceProvidedResources.len == 0)
	  recordComputeDispatches_targetOnly<CS, OD.TLS>(cmd);
	else
	  recordComputeDispatches_nested<CS, OD.TLS, OD.SLS>(cmd);
	recordComputeDispatches<layerIdx, LR, CSS.sub(1)>(cmd);
      }
    };

    template<size_t layerIdx> inline void renderFrom(vk::CommandBuffer cmd) {
      if constexpr(layerIdx < OD.LRS.len) {
	static constexpr layerRequirements LR = OD.LRS[layerIdx];
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier0 }>(cmd);
	recordCopies<LR.copies, LR>(cmd);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier1 }>(cmd);
	recordClears<LR.clears, LR>(cmd);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier2 }>(cmd);
	if constexpr(LR.renders)
	  recordRenders<layerIdx, OD.TLS, LR>(cmd);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier3 }>(cmd);
	recordComputeDispatches<layerIdx, LR, LR.computeShaders>(cmd);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier4 }>(cmd);
	renderFrom<layerIdx+1>(cmd);
      }
    };

    template<literalList<objectLayout> OLS> inline void doPrerender(vk::CommandBuffer cmd) {
      if constexpr(OLS.len) {
	constexpr objectLayout OL = OLS[0];
	for(auto* object : allObjects.template ofLayout<OL.id>()) {
	  object->preRender(cmd);
	}
	doPrerender<OLS.sub(1)>(cmd);
      }
    };

    template<literalList<objectLayout> OLS> inline void doPostrender(vk::SemaphoreSubmitInfo& renderWaitSem) {
      if constexpr(OLS.len) {
	constexpr objectLayout OL = OLS[0];
	if constexpr(object_t<OL.id>::needsPostrender) {
	  for(auto* object : allObjects.template ofLayout<OL.id>()) {
	    object->postRender(renderWaitSem);
	  }
	}
	doPostrender<OLS.sub(1)>(renderWaitSem);
      }
    };

    void waitForFrame(uint64_t frame, uint64_t timeoutNS = 10000000000) {//default = 10 seconds
      if(frame < this->frame - cmdFrameswapCount) {
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
#ifdef WITE_DEBUG_FENCES
      WARN("received signal on fence ", fenceIdx, " for frame ", frame, " current frame is ", this->frame);
#endif
      VK_ASSERT(dev->getVkDevice().waitForFences(1, &fences[fenceIdx], false, timeoutNS), "Frame timeout");
    };

    uint64_t render() {//returns frame number
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
      cmd.end();
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
      doPostrender<OD.TLS>(signalSem);//signaled by the render, waited by the presentation
      return frame++;
    };

  };

};
