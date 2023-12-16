#pragma once

#include <numeric>

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "iterableRecyclingPool.hpp"
#include "descriptorPoolPool.hpp"

namespace WITE {

  template<onionDescriptor OD> struct onion {

    static constexpr size_t cmdFrameswapCount = 2;//TODO LCM of all resources??

    uint64_t frame = 0;
    syncLock mutex;
    vk::CommandPool cmdPool;
    gpu* dev;
    vk::CommandBuffer primaryCmds[cmdFrameswapCount];
    vk::Fence fences[cmdFrameswapCount];
    vk::Semaphore semaphore;

    onion() {
      dev = &gpu::get(OD.GPUID);//note: this has a modulo op so GPUID does not need be < num of gpus on the system
      const vk::CommandPoolCreateInfo poolCI(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, dev->getQueueFam());
      VK_ASSERT(dev->getVkDevice().createCommandPool(&poolCI, ALLOCCB, &cmdPool), "failed to allocate command pool");
      vk::CommandBufferAllocateInfo allocInfo(cmdPool, vk::CommandBufferLevel::ePrimary, cmdFrameswapCount);
      VK_ASSERT(dev->getVkDevice().allocateCommandBuffers(&allocInfo, primaryCmds), "failed to allocate command buffer");
      vk::FenceCreateInfo fenceCI;//default
      for(size_t i = 0;i < cmdFrameswapCount;i++)
	VK_ASSERT(dev->getVkDevice().createFence(&fenceCI, ALLOCCB, &fences[i]), "failed to create fence");
      vk::SemaphoreTypeCreateInfo semTypeCI(vk::SemaphoreType::eTimeline);
      vk::SemaphoreCreateInfo semCI({}, &semTypeCI);
      VK_ASSERT(dev->getVkDevice().createSemaphore(&semCI, ALLOCCB, &semaphore), "failed to create semaphore");
    };

    template<resourceMap> struct mappedResourceTraits : std::false_type {};

    template<resourceMap RM> requires(containsId(OD.IRS, RM.requirementId)) struct mappedResourceTraits<RM> {
      static constexpr imageRequirements IR = findById(OD.IRS, RM.requirementId);
      typedef image<IR> type;
    };

    template<resourceMap RM> requires(containsId(OD.BRS, RM.requirementId)) struct mappedResourceTraits<RM> {
      static constexpr bufferRequirements BR = findById(OD.BRS, RM.requirementId);
      typedef buffer<BR> type;
    };

    template<literalList<resourceMap> RM>
    struct mappedResourceTuple {
      typedef mappedResourceTraits<RM[0]> MRT;
      MRT::type data;
      mappedResourceTuple<RM.sub(1)> rest;

      template<size_t IDX> inline auto& at() {
	if constexpr(IDX == 0) {
	  return data;
	} else {
	  return rest.template at<IDX-1>();
	}
      }

    };

    template<literalList<resourceMap> RM> requires(RM.len == 0) struct mappedResourceTuple<RM> : std::false_type {};

    static consteval size_t frameswapLCM(literalList<resourceMap> rms) {
      size_t ret = 1;
      for(const resourceMap& rm : rms)
	ret = std::lcm(ret, containsId(OD.IRS, rm.requirementId) ?
		       findById(OD.IRS, rm.requirementId).frameswapCount :
		       findById(OD.BRS, rm.requirementId).frameswapCount);
      return ret;
    };

    template<literalList<resourceMap> RMS> struct descriptorUpdateData_t {
      vk::WriteDescriptorSet writes[RMS.len];
      vk::DescriptorImageInfo images[RMS.countWhere([](resourceMap RM) { return containsId(OD.IRS, RM.requirementId); })];
      vk::DescriptorBufferInfo buffers[RMS.countWhere([](resourceMap RM) { return containsId(OD.BRS, RM.requirementId); })];
      uint32_t writeCount;//a shader may use a subset of target data
      uint32_t imageCount, bufferCount;//util for filling this struct
    };

    template<literalList<resourceMap> RMS, literalList<resourceReference> RRS>
    inline void fillWrites(descriptorUpdateData_t<RMS>& data, mappedResourceTuple<RMS>& rm, vk::DescriptorSet ds,
			   size_t frameMod) {
      static_assert(RRS.len <= RMS.len);//because RMS.len is the size of data.writes.  MAYBE need more if multiple RR per RM
      if constexpr(RRS.len) {
	static constexpr resourceReference RR = RRS[0];
	static constexpr resourceMap RM = *findResourceReferencing(RMS, RR.id);
	static constexpr size_t RMX = findId(RMS, RM.id);
	static_assert(RR.usage.type == resourceUsageType::eDescriptor);
	if constexpr(RR.access & accessFlagsDenotingDescriptorUsage) {
	  auto& w = data.writes[data.writeCount];
	  w.descriptorSet = ds;
	  w.dstBinding = data.writeCount;
	  w.dstArrayElement = 0;
	  w.descriptorCount = 1;
	  w.descriptorType = RR.usage.asDescriptor.descriptorType;
	  if constexpr(containsId(OD.IRS, rm.requirementId)) {
	    auto& img = data.images[data.imageCount++];
	    w.pImageInfo = &img;
	    if constexpr(RR.usage.asDescriptor.descriptorType == vk::DescriptorType::eCombinedImageSampler ||
			 RR.usage.asDescriptor.descriptorType == vk::DescriptorType::eSampler) {
	      img.sampler = dev->getSampler<RR.usage.asDescriptor.sampler>();
	    }
	    img.imageView = rm.template at<RMX>().createView(frameMod);
	    img.imageLayout = imageLayoutFor(RR.access);
	  } else {
	    auto& buf = data.buffers[data.bufferCount++];
	    w.pBufferInfo = &buf;
	    buf.buffer = rm.template at<RMX>().frameBuffer(frameMod);
	    buf.offset = 0;
	    buf.range = findById(OD.BRS, RM.requirementId).size;
	  }
	  data.writeCount++;
	}
	fillWrite<RMS, RRS.sub(1)>(data, rm, ds, frameMod);
      }
    };

    template<uint64_t TARGET_ID> class target_t {
    public:
      static constexpr size_t TARGET_IDX = findId(OD.TLS, TARGET_ID);
      static constexpr targetLayout TL = OD.TLS[TARGET_IDX];
      static constexpr size_t maxFrameswap = frameswapLCM(TL.resources);

    private:
      struct perShaderBundle {
	descriptorUpdateData_t<TL.resources> descriptorUpdateData;
	vk::DescriptorSet descriptorSet;
      };

      onion<OD>* o;
      mappedResourceTuple<TL.resources> resources;
      std::map<uint64_t, frameBufferBundle[maxFrameswap]> fbByRpIdByFrame;
      std::map<uint64_t, perShaderBundle[maxFrameswap]> perShaderByIdByFrame;

      target_t(const target_t&) = delete;
      target_t() = delete;
      target_t(onion<OD>* o): o(o) {};

      friend onion;

    public:

      template<uint64_t resourceMapId> auto& get() {
	return resources.template at<findId(TL.resources, resourceMapId)>();
      };

      template<uint64_t resourceMapId> void write(auto t) {
	scopeLock lock(&o->mutex);
	get<resourceMapId>().set(o->frame + findById(TL.resources, resourceMapId).hostAccessOffset, t);
      };

    };

    template<size_t IDX = 0> struct targetCollectionTuple {
      iterableRecyclingPool<std::unique_ptr<target_t<OD.TLS[IDX].id>>> data;
      targetCollectionTuple<IDX+1> rest;
      template<size_t TID, uint64_t TIDX = findId(OD.TLS, TID)> auto& ofLayout() {
	if constexpr(IDX == TIDX)
	  return data;
	else
	  return rest.template ofLayout<TID, TIDX>();
      };
    };

    template<size_t IDX> requires(IDX == OD.TLS.len) struct targetCollectionTuple<IDX> : std::false_type {};

    targetCollectionTuple<> allTargets;

    template<uint64_t targetId> target_t<targetId>* createTarget() {
      scopeLock lock(&mutex);
      std::unique_ptr<target_t<targetId>>* ret = allTargets.template ofLayout<targetId>().allocate();
      if(!*ret) [[unlikely]]
	ret->reset(new target_t<targetId>(this));
#error TODO initialize: transition all images into their FINAL layout (so the first transition in the onion has the expected oldLayout)
      return ret->get();
    };

    template<uint64_t targetId> void freeTarget(target_t<targetId>* d) {
      scopeLock lock(&mutex);
      allTargets.template ofLayout<targetId>().free(d);
    };

    template<uint64_t SOURCE_ID> class source_t {
    public:
      static constexpr size_t SOURCE_IDX = findId(OD.SLS, SOURCE_ID);
      static constexpr sourceLayout SOURCE_PARAMS = OD.SLS[SOURCE_IDX];
      static constexpr size_t maxFrameswap = frameswapLCM(SOURCE_PARAMS.resources);

    private:
      struct perShaderBundle {
	descriptorUpdateData_t<SOURCE_PARAMS.resources> descriptorUpdateData;
	vk::DescriptorSet descriptorSet;
      };

      onion<OD>* o;
      mappedResourceTuple<SOURCE_PARAMS.resources> resources;
      std::map<uint64_t, perShaderBundle[maxFrameswap]> perShaderByIdByFrame;

      source_t(const source_t&) = delete;
      source_t() = delete;
      source_t(onion<OD>* o): o(o) {};

      friend onion;

    public:

      template<uint64_t resourceMapId> auto& get() {
	return resources.template at<findId(SOURCE_PARAMS.resources, resourceMapId)>();
      };

      template<uint64_t resourceMapId> void write(auto t) {
	scopeLock lock(&o->mutex);
	get<resourceMapId>().set(o->frame + findById(SOURCE_PARAMS.resources, resourceMapId).hostAccessOffset, t);
      };

    };

    template<size_t IDX = 0> struct sourceCollectionTuple {
      iterableRecyclingPool<std::unique_ptr<source_t<OD.SLS[IDX].id>>> data;
      sourceCollectionTuple<IDX+1> rest;
      template<size_t TID, uint64_t TIDX = findId(OD.SLS, TID)> auto& ofLayout() {
	if constexpr(IDX == TIDX)
	  return data;
	else
	  return rest.template ofLayout<TID, TIDX>();
      };
    };

    template<size_t IDX> requires(IDX == OD.SLS.len) struct sourceCollectionTuple<IDX> : std::false_type {};

    sourceCollectionTuple<> allSources;

    template<uint64_t sourceId> source_t<sourceId>* createSource() {
      scopeLock lock(&mutex);
      std::unique_ptr<source_t<sourceId>>* ret = allSources.template ofLayout<sourceId>().allocate();
      if(!*ret) [[unlikely]]
	ret->reset(new source_t<sourceId>(this));
#error TODO initialize: transition all images into their FINAL layout (so the first transition in the onion has the expected oldLayout)
      return ret->get();
    };

    template<uint64_t sourceId> void freeSource(source_t<sourceId>* d) {
      scopeLock lock(&mutex);
      allSources.template ofLayout<sourceId>().free(d);
    };

    static consteval std::vector<resourceAccessTime> findUsages(resourceMap RM, uint64_t layoutId) {
      std::vector<resourceAccessTime> ret;
      for(size_t layerIdx = 0;layerIdx < OD.LRS.len;layerIdx++) {
	const layerRequirements& layer = OD.LRS[layerIdx];
	if(!layer.sourceLayouts.contains(layoutId) && !layer.targetLayouts.contains(layoutId))
	  continue;
	//copy:
	for(size_t substepIdx : layer.copies) {
	  const auto& substep = findById(OD.CSS, substepIdx);
	  if(RM.resourceReferences.contains(substep.src.id))
	    ret.push_back({ layerIdx, substep_e::copy, { substep.src.id, {}, vk::AccessFlagBits2::eTransferRead, substep.src.frameLatency }});
	  if(RM.resourceReferences.contains(substep.dst.id))
	    ret.push_back({ layerIdx, substep_e::copy, { substep.dst.id, {}, vk::AccessFlagBits2::eTransferWrite, substep.src.frameLatency }});
	}
	constexpr size_t urrLen = std::numeric_limits<decltype(resourceBarrier::frameLatency)>::max();
	static_assert(std::numeric_limits<decltype(resourceBarrier::frameLatency)>::min() == 0);
	//rendering:
	for(size_t substepIdx : layer.renders) {
	  const auto& substep = findById(OD.RPRS, substepIdx);
	  if(RM.resourceReferences.contains(substep.depthStencil.id))
	    ret.push_back({ layerIdx, substep_e::render, substep.depthStencil });
	  if(RM.resourceReferences.contains(substep.color.id))
	    ret.push_back({ layerIdx, substep_e::render, substep.color });
	  resourceReference urr[256];
	  for(const graphicsShaderRequirements& gsr : substep.shaders) {
	    for(const resourceReference& srr : gsr.targetProvidedResources)
	      if(RM.resourceReferences.contains(srr.id))
		urr[srr.frameLatency] |= srr;
	    for(const resourceReference& srr : gsr.sourceProvidedResources)
	      if(RM.resourceReferences.contains(srr.id))
		urr[srr.frameLatency] |= srr;
	  }
	  for(size_t fl = 0;fl < urrLen;fl++)
	    if(urr[fl].access)
	      ret.push_back({ layerIdx, substep_e::render, urr[fl] });
	}
	//compute:
	resourceReference urr[urrLen];
	for(size_t csrId : layer.computeShaders) {
	  const computeShaderRequirements& csr = findById(OD.CSRS, csrId);
	  for(const resourceReference& srr : csr.targetProvidedResources)
	    if(RM.resourceReferences.contains(srr.id))
	      urr[srr.frameLatency] |= srr;
	  for(const resourceReference& srr : csr.sourceProvidedResources)
	    if(RM.resourceReferences.contains(srr.id))
	      urr[srr.frameLatency] |= srr;
	}
	for(size_t fl = 0;fl < urrLen;fl++)
	  if(urr[fl].access)
	    ret.push_back({ layerIdx, substep_e::render, urr[fl] });
      }
      std::sort(ret.begin(), ret.end());
      return ret;
    };

    static consteval std::vector<resourceBarrier> findBarriers(resourceMap RM, uint64_t layoutId) {
      auto usage = findUsages(RM, layoutId);
      size_t afterIdx = 0, beforeIdx = usage.size()-1;
      std::vector<resourceBarrier> ret;
      if(usage.size() < 2) return ret;
      while(afterIdx < usage.size()) {
	resourceBarrier rb;
	rb.before = usage[beforeIdx];
	rb.after = usage[afterIdx];
	rb.resourceId = RM.id;
	rb.requirementId = RM.requirementId;
	rb.layoutId = layoutId;
	constexpr_assert(rb.before < rb.after || afterIdx == 0);
	rb.timing.layerIdx = rb.after.layerIdx;
	rb.frameLatency = rb.after.usage.frameLatency;
	if(rb.before.layerIdx != rb.after.layerIdx) {//prefer top of layer for barriers
	  rb.timing.substep = substep_e::barrier1;
	} else {
	  constexpr_assert(rb.after.substep != rb.before.substep);
	  rb.timing.substep = (substep_e)((int)rb.after.substep - 1);//otherwise wait as long as possible
	}
	ret.push_back(rb);
	afterIdx++;
	beforeIdx = afterIdx-1;
      }
      return ret;
    };

    static consteval std::vector<resourceBarrier> allBarriers() {
      std::vector<resourceBarrier> allBarriers;
      for(size_t layerIdx = 0;layerIdx < OD.LRS.len;layerIdx++) {
	for(sourceLayout SL : OD.SLS)
	  for(resourceMap RM : SL.resources)
	    concat(allBarriers, findBarriers(RM, SL.id));
	for(targetLayout TL : OD.TLS)
	  for(resourceMap RM : TL.resources)
	    concat(allBarriers, findBarriers(RM, TL.id));
      }
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
	  constexpr copyableArray<vk::ImageMemoryBarrier2, barrierBatchSize> baseline(vk::ImageMemoryBarrier2 {
	      toPipelineStage2(RB.before.usage.stages),
	      RB.before.usage.access,
	      toPipelineStage2(RB.after.usage.stages),
	      RB.after.usage.access,
	      imageLayoutFor(RB.before.usage.access),
	      imageLayoutFor(RB.after.usage.access),
	      {}, {}, {},
	      getAllInclusiveSubresource(findById(OD.IRS, RB.requirementId))
	    });
	  copyableArray<vk::ImageMemoryBarrier2, barrierBatchSize> barriers = baseline;
	  DI.pImageMemoryBarriers = barriers.ptr();
	  DI.imageMemoryBarrierCount = barrierBatchSize;
	  size_t mbIdx = 0;
	  for(auto& cluster : getAllOfLayout<RB.layoutId>()) {
	    barriers[mbIdx].image = cluster->get()->template get<RB.resourceId>().frameImage(RB.frameLatency + frame);
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
	  constexpr copyableArray<vk::BufferMemoryBarrier2, barrierBatchSize> baseline(vk::BufferMemoryBarrier2 {
	      toPipelineStage2(RB.before.usage.stages),
	      RB.before.usage.access,
	      toPipelineStage2(RB.after.usage.stages),
	      RB.after.usage.access,
	      {}, {}, {}, 0, VK_WHOLE_SIZE
	    });
	  copyableArray<vk::BufferMemoryBarrier2, barrierBatchSize> barriers = baseline;
	  DI.pBufferMemoryBarriers = barriers.ptr();
	  DI.bufferMemoryBarrierCount = barrierBatchSize;
	  size_t mbIdx = 0;
	  for(auto& cluster : getAllOfLayout<RB.layoutId>()) {
	    barriers[mbIdx].buffer = cluster->get()->template get<RB.resourceId>().frameBuffer(RB.frameLatency + frame);
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
	static constexpr auto XL = findLayoutById<XLS[0]>();
	static constexpr const resourceMap * SRM = findResourceReferencing(XL.resources, CS.src.id),
	  *DRM = findResourceReferencing(XL.resources, CS.dst.id);
	if constexpr(SRM && DRM) {
	  static constexpr bool srcIsImage = containsId(OD.IRS, SRM->requirementId),
	    dstIsImage = containsId(OD.IRS, DRM->requirementId);
	  static_assert(!(srcIsImage ^ dstIsImage));//NYI buffer to/from image (why would you do that?)
	  if constexpr(srcIsImage && dstIsImage) {
	    //always blit, no way to know at compile time if they're the same size
	    std::array<vk::Offset3D, 2> srcBounds, dstBounds;
	    vk::ImageBlit blitInfo(getAllInclusiveSubresourceLayers(findById(OD.IRS, SRM->requirementId)),
				   srcBounds,
				   getAllInclusiveSubresourceLayers(findById(OD.IRS, DRM->requirementId)),
				   dstBounds);
	    for(auto& cluster : getAllOfLayout<XL.id>()) {
	      auto& src = cluster->get()->template get<SRM->id>();
	      auto& dst = cluster->get()->template get<DRM->id>();
	      blitInfo.srcOffsets = src.getSize();
	      blitInfo.dstOffsets = dst.getSize();
	      cmd.blitImage(src.frameImage(CS.src.frameLatency + frame),
			    imageLayoutFor(vk::AccessFlagBits2::eTransferRead),
			    dst.frameImage(CS.dst.frameLatency + frame),
			    imageLayoutFor(vk::AccessFlagBits2::eTransferWrite),
			    1, &blitInfo, CS.filter);
	    }
	  } else {//buffer to buffer
	    static constexpr bufferRequirements SBR = findById(OD.BRS, SRM->requirementId),
	      TBR = findById(OD.BRS, DRM->requirementId);
	    static constexpr vk::BufferCopy copyInfo(0, 0, min(SBR.size, TBR.size));
	    for(auto& cluster : getAllOfLayout<XL.id>()) {
	      auto& src = cluster->get()->template get<SRM->id>();
	      auto& dst = cluster->get()->template get<DRM->id>();
	      cmd.copyBuffer(src.frameBuffer(CS.src.frameLatency + frame),
			     dst.frameBuffer(CS.dst.frameLatency + frame),
			     1, &copyInfo);
	    }
	  }
	}
	recordCopies<CS, XLS.sub(1)>(cmd);
      }
    };

    template<literalList<uint64_t> CSIDS, layerRequirements LR> inline void recordCopies(vk::CommandBuffer cmd) {
      if constexpr(CSIDS.len) {
	static constexpr copyStep CS = findById(OD.CSS, CSIDS[0]);
	recordCopies<CS, LR.targetLayouts>(cmd);
	recordCopies<CS, LR.sourceLayouts>(cmd);
	recordCopies<CSIDS.sub(1), LR>(cmd);
      }
    };

    template<renderPassRequirements RP, targetLayout TL> struct renderPassInfoBundle {
      static constexpr const resourceMap *colorRM = findResourceReferencing(TL.resources, RP.color.id),
	*depthRM = findResourceReferencing(TL.resources, RP.depthStencil.id);
      static constexpr imageRequirements colorIR = findById(OD.IRS, colorRM->requirementId),
	depthIR = findById(OD.IRS, depthRM->requirementId);
      static constexpr vk::AttachmentReference colorRef { 0, imageLayoutFor(RP.color.access) },
	depthRef { 1, imageLayoutFor(RP.depthStencil.access) };
      static constexpr vk::SubpassDescription subpass { {}, vk::PipelineBindPoint::eGraphics, 0, NULL, 1, &colorRef, NULL, &depthRef };
      static constexpr vk::AttachmentDescription attachments[2] = {//MAYBE variable attachments when inputs are allowed
	{ {}, colorIR.format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, imageLayoutFor(RP.color.access), imageLayoutFor(RP.color.access) },
	{ {}, depthIR.format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, imageLayoutFor(RP.depthStencil.access), imageLayoutFor(RP.depthStencil.access) }
      };
      //TODO multiview
      //static constexpr vk::RenderPassMultiviewCreateInfo multiview {
      static constexpr vk::RenderPassCreateInfo rpci { {}, 2, attachments, 1, &subpass, 0 };
    };

    //TODO make an init function, copy the recursive structure, and premake all the pipes and stuff (after successful render)
    //MAYBE match like clusters of shaders, sources, and targets so they can share layouts and descriptor pools?

    template<targetLayout TL, graphicsShaderRequirements GSR, literalList<uint64_t> SLIDS>
    inline void recordRenders(auto& target, perTargetLayout& ptl, perTargetLayoutPerShader& ptlps, onionStaticData& od, target_t<TL.id>::perShaderBundle& perShader, vk::RenderPass rp, vk::CommandBuffer cmd) {
      if constexpr(SLIDS.len) {
	static constexpr sourceLayout SL = findById(OD.SLS, SLIDS[0]);
	//for now (at least) only one vertex binding of each input rate type. No instance without vertex. Source only.
	static constexpr auto findVB = [](resourceReference& rr) {
	  return rr.usage.type == resourceUsageType::eVertex &&
	    rr.usage.asVertex.rate == vk::VertexInputRate::eVertex &&
	    findResourceReferencing(SL.resources, rr.id);
	};
	static constexpr auto findIB = [](resourceReference& rr) {
	  return rr.usage.type == resourceUsageType::eVertex &&
	    rr.usage.asVertex.rate == vk::VertexInputRate::eInstance &&
	    findResourceReferencing(SL.resources, rr.id);
	};
	static_assert(GSR.sourceProvidedResources.countWhere(findVB) == 1);
	static_assert(GSR.sourceProvidedResources.countWhere(findIB) <= 1);
	static constexpr resourceReference* vb = GSR.sourceProvidedResources.firstWhere(findVB);
	static constexpr resourceReference* ib = GSR.sourceProvidedResources.firstWhere(findIB);
	perTargetLayoutPerSourceLayoutPerShader& shaderInstance = ptlps.perSL[SL];
	perSourceLayoutPerShader& pslps = od.perSL[SL.id].perShader[GSR.id];
	if(!pslps.descriptorPool) [[unlikely]]
	  pslps.descriptorPool.reset(new descriptorPoolPool<GSR.sourceProvidedResources, OD.GPUID>());
	if(!shaderInstance.pipeline) [[unlikely]] {
	  auto vkDev = dev->getVkDevice();
	  vk::DescriptorSetLayout dsls[2] { pslps.descriptorPool->getDSL(), ptlps.descriptorPool->getDSL() };
	  vk::PipelineLayoutCreateInfo plci { {}, 2, dsls };//NOTE: push constants NYI, supply here if needed
	  VK_ASSERT(vkDev.createPipelineLayout(&plci, ALLOCCB, &shaderInstance.pipelineLayout),
		    "Failed to create pipeline layout; source: ", SL.id, " target: ", TL.id);
	  vk::ShaderModuleCreateInfo moduleCI;
	  vk::ShaderModule module;
	  vk::PipelineShaderStageCreateInfo stages[GSR.modules.len];
	  for(size_t i;i < GSR.modules.len;i++) {
	    moduleCI.setPCode(GSR.modules[i].data)
	      .setCodeSize(GSR.modules[i].size);
	    VK_ASSERT(vkDev.createShaderModule(&moduleCI, ALLOCCB, &module), "Failed to create shader module");
	    stages[i].setStage(GSR.modules[i].stage)
	      .setModule(module);
	  }
	  static constexpr size_t vibCount = ib ? 2 : vb ? 1 : 0;
	  static constexpr vk::VertexInputBindingDescription vibs[2] = {
	    getBindingDescription<vb, 0>(),
	    getBindingDescription<ib, 1>()
	  };
	  static constexpr copyableArray<resourceReference, vibCount> vib = [](size_t i){ return i ? *ib : *vb; };
	  static constexpr auto viad = getAttributeDescriptions<vib>();
	  static constexpr vk::PipelineVertexInputStateCreateInfo verts { {}, vibCount, vibs, viad.LEN, viad.ptr() };
	  static constexpr vk::PipelineInputAssemblyStateCreateInfo assembly { {}, GSR.topology, false };
	  //TODO tessellation
	  //viewport and scissors are dynamic so we don't need to rebuild the pipe when the target size changes
	  static constexpr vk::PipelineViewportStateCreateInfo vp = { {}, 1, NULL, 1, NULL };
	  static constexpr vk::PipelineRasterizationStateCreateInfo raster = { {}, false, false, vk::PolygonMode::eFill, GSR.cullMode, GSR.windCounterClockwise ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise };//not set: depth bias stuff, line width
	  static constexpr vk::PipelineMultisampleStateCreateInfo multisample = { {}, vk::SampleCountFlagBits::e1, 0, 0, NULL, 0, 0 };
	  static constexpr vk::PipelineDepthStencilStateCreateInfo depth = { {}, true, true, vk::CompareOp::eLess, false, false };//not set: depth bounds and stencil test stuff
	  static constexpr vk::PipelineColorBlendAttachmentState blendAttachment = { false };
	  static constexpr vk::PipelineColorBlendStateCreateInfo blend = { {}, false, {}, 1, &blendAttachment, { 1, 1, 1, 1 } };
	  static constexpr vk::DynamicState dynamics[] = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
	  static constexpr vk::PipelineDynamicStateCreateInfo dynamic = { {}, 2, dynamics };
	  vk::GraphicsPipelineCreateInfo gpci { {}, GSR.modules.len, stages, &verts, /*tesselation*/ NULL, &vp, &raster, &multisample, &depth, &blend, &dynamic, shaderInstance.pipelineLayout, rp, /*subpass idx*/ 0 };//not set: derivitive pipeline stuff
	  VK_ASSERT(vkDev.createGraphicsPipelines(dev->getPipelineCache(), 1, &gpci, ALLOCCB, &shaderInstance.pipeline),
		    "Failed to create graphics pipeline ", GSR.id);
	}
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderInstance.pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 1, 1, &perShader.descriptorSet, 0, NULL);
	for(auto& source : allSources.template ofLayout<SL.id>()) {
	  size_t frameMod = frame % source_t<SL.id>::maxFrameswap;
	  auto& sourcePerShaderPerFrame = source.perShaderByIdByFrame[GSR.id][frameMod];
	  if(!sourcePerShaderPerFrame.descriptorSet) [[unlikely]] {
	    sourcePerShaderPerFrame.descriptorSet = od.perSL[SL.id].perShader[GSR.id].descriptorPool->allocate();
	    fillWrites<SL.resources, GSR.sourceProvidedResources>
	      (sourcePerShaderPerFrame.descriptorUpdateData, source.resources, sourcePerShaderPerFrame.descriptorSet, frameMod);
	    VK_ASSERT(dev->getVkDevice().updateDescriptorSets(sourcePerShaderPerFrame.descriptorUpdateData.writeCount,
							      sourcePerShaderPerFrame.descriptorUpdateData.writes),
		      "Failed to udpate source descriptor set");
	  }
	  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderInstance.pipelineLayout, 0, 1, &sourcePerShaderPerFrame.descriptorSet, 0, NULL);
	  static constexpr vk::DeviceSize zero = 0;//offsets below
	  static constexpr resourceMap vbm = *findResourceReferencing(SL.resources, vb->id);
	  static constexpr const resourceMap* ibm = findResourceReferencing(SL.resources, ib->id);
	  cmd.bindVertexBuffers(0, 1, source.template get<vbm.id>().frameBuffer(frame + vb->frameLatency), &zero);
	  if constexpr(ibm)
	    cmd.bindVertexBuffers(1, 1, source.template get<ibm->id>().frameBuffer(frame + ib->frameLatency), &zero);
	  cmd.draw(findById(OD.BRS, vbm.requirementId).size / sizeofUdm<vb->usage.asVertex.format>(), ibm ? (findById(OD.BRS, ibm->requirementId).size / sizeofUdm<ib->usage.asVertex.format>()) : 1, 0, 0);
	  //TODO more flexibility with draw. Allow source layout to ask for multi-draw, indexed, indirect etc. Allow allow (dynamic) less than the whole buffer.
	}
	recordRenders<TL, GSR, SLIDS.sub(1)>(target, ptl, ptlps, od, perShader, cmd);
      }
    };

    template<targetLayout TL, layerRequirements LR, literalList<graphicsShaderRequirements> GSRS>
    inline void recordRenders(auto& target, perTargetLayout& ptl, onionStaticData& od, vk::RenderPass rp, vk::CommandBuffer cmd) {
      if constexpr(GSRS.len) {
	static constexpr graphicsShaderRequirements GSR = GSRS[0];
	size_t frameMod = frame % target_t<TL.id>::maxFrameswap;
	auto& perShaderPerFrame = target.perShaderByIdByFrame[GSR.id][frameMod];
	perTargetLayoutPerShader& ptlps = ptl.perShader[GSR.id];
	if(!perShaderPerFrame.descriptorSet) [[unlikely]] {
	  if(!ptlps.descriptorPool) [[unlikely]]
	    ptlps.descriptorPool.reset(new descriptorPoolPool<GSR.targetProvidedResources, OD.GPUID>());
	  perShaderPerFrame.descriptorSet = ptlps.descriptorPool->allocate();
	  fillWrites<TL.resources, GSR.targetProvidedResources>
	    (perShaderPerFrame.descriptorUpdateData, target.resources, perShaderPerFrame.descriptorSet, frameMod);
	  VK_ASSERT(dev->getVkDevice().updateDescriptorSets(perShaderPerFrame.descriptorUpdateData.writeCount,
							    perShaderPerFrame.descriptorUpdateData.writes),
		    "Failed to udpate descriptor set");
	}
	//TODO dirty check descriptors and update if needed (changing which resources a DS points to is very rare)
	recordRenders<TL, GSR, LR.sourceLayouts>(target, ptl, ptlps, od, perShaderPerFrame, rp, cmd);
	recordRenders<TL, LR, GSRS.sub(1)>(target, ptl, od, rp, cmd);
      }
    };

    template<targetLayout TL, layerRequirements LR, literalList<uint64_t> RPIDS>
    inline void recordRenders(auto& target, perTargetLayout& ptl, onionStaticData& od, vk::CommandBuffer cmd) {
      if constexpr(RPIDS.len) {
	static constexpr renderPassRequirements RP = findById(OD.RPRS, RPIDS[0]);
	vk::RenderPass& rp = ptl.rpByRequirementId[RP.id];
	if(!rp) [[unlikely]] {
	  VK_ASSERT(dev->getVkDevice().createRenderPass(&renderPassInfoBundle<RP, TL>::rpci, ALLOCCB, &rp),
		    "failed to create render pass ", TL.id, " ", RP.id);
	}
	static constexpr const resourceMap* colorRM = findResourceReferencing(TL.resources, RP.color.id),
	  *depthRM = findResourceReferencing(TL.resources, RP.depthStencil.id);
	auto& color = target.template get<colorRM->id>(), depth = target.template get<depthRM->id>();
	vk::Rect2D size = color.getSizeRect();
	frameBufferBundle& fbb = target.fbByRpIdByFrame[RP.id][frame % target_t<TL.id>::maxFrameswap];
	if(!fbb.fb) [[unlikely]] {
	  fbb.fbci.renderPass = rp;
	  fbb.fbci.width = size.extent.width;
	  fbb.fbci.height = size.extent.height;
	  fbb.fbci.layers = 1;
	  fbb.attachments[0] = color.createView(frame + RP.color.frameLatency);
	  fbb.attachments[1] = depth.createView(frame + RP.depthStencil.frameLatency);
	  VK_ASSERT(dev->getVkDevice().createFramebuffer(&fbb.fbci, ALLOCCB, &fbb.fb), "failed to create framebuffer");
	}
	vk::RenderPassBeginInfo rpBegin(rp, fbb.fb, size, 0);
	cmd.beginRenderPass(&rpBegin, vk::SubpassContents::eInline);
	recordRenders<TL, LR, RP.shaders>(target, ptl, od, rp, cmd);
	cmd.endRenderPass();
	recordRenders<TL, LR, RPIDS.sub(1)>(target, ptl, od, cmd);
      }
    };

    template<targetLayout TL, layerRequirements LR>
    inline void recordRenders(perTargetLayout& ptl, onionStaticData& od, vk::CommandBuffer cmd) {
      //TODO spawn each of these in a parallel cmd; rework call stack signatures to hand semaphores around instead of cmd
      //^^  ONLY if there is no writable source descriptor
      for(auto& target : allTargets.template ofLayout<layoutId>()) {
	#error TODO set scissors and viewport here
	recordRenders<TL, LR, LR.renders>(target, ptl, od, cmd);
      }
    };

    template<literalList<uint64_t> TLS, layerRequirements LR> inline void recordRenders(vk::CommandBuffer cmd) {
      if constexpr(TLS.len) {
	static constexpr targetLayout TL = findById(OD.TLS, TLS[0]);
	scoptLock lock(&onionStaticData::allDataMutex);
	onionStaticData& od = onionStaticData::allOnionData[OD];
	perTargetLayout& ptl = od.perTL[TL];
	lock.release();
	recordRenders<TL, LR>(ptl, od, cmd);
	recordRenders<TLS.sub(1), LR>(cmd);
      };
    };

    template<size_t layerIdx> inline void renderFrom(vk::CommandBuffer cmd) {
      if constexpr(layerIdx < OD.LRS.len) {
	static constexpr layerRequirements LR = OD.LRS[layerIdx];
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier1 }>(cmd);
	recordCopies<LR.copies, LR>(cmd);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier2 }>(cmd);
	if constexpr(LR.renders)
	  recordRenders<LR.targetLayouts, LR>(cmd);
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier3 }>(cmd);
	//TODO compute
	recordBarriersForTime<resourceBarrierTiming { .layerIdx = layerIdx, .substep = substep_e::barrier4 }>(cmd);
	renderFrom<layerIdx+1>(cmd);
      }
    };

    void render() {
      //TODO make some secondary cmds
      //TODO cache w/ dirty check the cmds
      scopeLock lock(&mutex);
      vk::CommandBuffer cmd = primaryCmds[frame % cmdFrameswapCount];
      VK_ASSERT(cmd.reset({}), "failed to reset command buffer");
      renderFrom<0>(cmd);
      vk::Fence fence = fences[frame % cmdFrameswapCount];
      vk::SemaphoreSubmitInfo waitSem(semaphore, frame-1, vk::PipelineStageFlagBits2::eAllCommands),
	signalSem(semaphore, frame, vk::PipelineStageFlagBits2::eAllCommands);
      vk::CommandBufferSubmitInfo cmdSubmitInfo(cmd);
      vk::SubmitInfo2 submit({}, frame ? 1 : 0, &waitSem, 1, &cmdSubmitInfo, 1, &signalSem);
      VK_ASSERT(dev->getQueue().submit2(1, &submit, fence), "failed to submit command buffer");
    }

  };

};
