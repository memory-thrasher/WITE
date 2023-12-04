#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "iterableRecyclingPool.hpp"

namespace WITE {

  template<onionDescriptor OD> struct onion {

    uint64_t frame = 0;
    syncLock mutex;

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

    template<uint64_t TARGET_ID> class target_t {
    public:
      static constexpr size_t TARGET_IDX = findId(OD.TLS, TARGET_ID);
      static constexpr targetLayout TL = OD.TLS[TARGET_IDX];

    private:
      onion<OD>* o;
      mappedResourceTuple<TL.targetProvidedResources> resources;

      target_t(const target_t&) = delete;
      target_t() = delete;
      target_t(onion<OD>* o): o(o) {};

      friend onion;

    public:

      template<uint64_t resourceMapId> auto& get() {
	return resources.template at<findId(TL.targetProvidedResources, resourceMapId)>();
      };

      template<uint64_t resourceMapId> void write(auto t) {
	scopeLock lock(&o->mutex);
	get<resourceMapId>().set(o->frame + findById(TL.targetProvidedResources, resourceMapId).hostAccessOffset, t);
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
      if(!*ret)
	ret->reset(new target_t<targetId>(this));
      //TODO initialize: transition all images into their FINAL layout (so the first transition in the onion has the expected oldLayout)
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

    private:
      onion<OD>* o;
      mappedResourceTuple<SOURCE_PARAMS.sourceProvidedResources> resources;

      source_t(const source_t&) = delete;
      source_t() = delete;
      source_t(onion<OD>* o): o(o) {};

      friend onion;

    public:

      template<uint64_t resourceMapId> auto& get() {
	return resources.template at<findId(SOURCE_PARAMS.sourceProvidedResources, resourceMapId)>();
      };

      template<uint64_t resourceMapId> void write(auto t) {
	scopeLock lock(&o->mutex);
	get<resourceMapId>().set(o->frame + findById(SOURCE_PARAMS.sourceProvidedResources, resourceMapId).hostAccessOffset, t);
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
      if(!*ret)
	ret->reset(new source_t<sourceId>(this));
      //TODO initialize: transition all images into their FINAL layout (so the first transition in the onion has the expected oldLayout)
      return ret->get();
    };

    template<uint64_t sourceId> void freeSource(source_t<sourceId>* d) {
      scopeLock lock(&mutex);
      allSources.template ofLayout<sourceId>().free(d);
    };

    consteval std::vector<resourceUsage> findUsages(resourceMap RM, uint64_t layoutId) {
      std::vector<resourceUsage> ret;
      for(size_t layerIdx = 0;layerIdx < OD.LRS.len;layerIdx++) {
	const layerRequirements& layer = OD.LRS[layerIdx];
	if(!layer.sourceLayouts.contains(layoutId) && !layer.targetLayouts.contains(layoutId))
	  continue;
	//copy:
	for(size_t substepIdx : layer.copies) {
	  const auto& substep = findById(OD.CSS, substepIdx);
	  if(RM.resourceReferences.contains(substep.src.id))
	    ret.push_back({ layerIdx, substep_e::copy, { substep.src.id, vk::PipelineStageFlagBits2::eAllTransfer, {}, vk::AccessFlagBits2::eTransferRead, substep.src.frameLatency }});
	  if(RM.resourceReferences.contains(substep.dst.id))
	    ret.push_back({ layerIdx, substep_e::copy, { substep.dst.id, {}, vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite, substep.src.frameLatency }});
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

    consteval std::vector<resourceBarrier> findBarriers(resourceMap RM, uint64_t layoutId) {
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

    consteval std::vector<resourceBarrier> allBarriers() {
      std::vector<resourceBarrier> allBarriers;
      for(size_t layerIdx = 0;layerIdx < OD.LRS.len;layerIdx++) {
	for(sourceLayout SL : OD.SLS)
	  for(resourceMap RM : SL.sourceProvidedResources)
	    concat(allBarriers, findBarriers(RM, SL.id));
	for(targetLayout TL : OD.TLS)
	  for(resourceMap RM : TL.targetProvidedResources)
	    concat(allBarriers, findBarriers(RM, TL.id));
      }
      return allBarriers;
    };

    consteval std::vector<resourceBarrier> barriersForTime(resourceBarrierTiming BT) {
      auto ret = allBarriers();
      std::erase_if(ret, [BT](auto x){ return x.timeing != BT; });
      return ret;
    };

    template<literalList<resourceBarrier> RBS> inline void doBarriers() {
      if constexpr(RBS.len) {
	constexpr size_t barrierBatchSize = 256;
	constexpr resourceBarrier RB = RBS[0];
	constexpr bool isImage = containsId(OD.IRS, RB.requirementId);
	vk::DependencyInfo DI;
	if constexpr(isImage) {
	  constexpr copyableArray<vk::ImageMemoryBarrier2, barrierBatchSize> baseline(vk::ImageMemoryBarrier2 {
	      RB.before.usage.readStages | RB.before.usage.writeStages,
	      RB.before.usage.access,
	      RB.after.usage.readStages | RB.after.usage.writeStages,
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
	  if constexpr(containsId(OD.SLS, RB.layoutId)) {
	    for(auto& cluster : allSources.template ofLayout<RB.layoutId>()) {
	      barriers[mbIdx % barrierBatchSize].image = cluster.template get<RB.resourceId>().frameImage(RB.frameLatency + frame);
	      mbIdx++;
	      if(mbIdx % barrierBatchSize == 0)
		cmd.pipelineBarrier2(&DI);
	    }
	    if(mbIdx) {
	      DI.imageMemoryBarrierCount = mbIdx;
	      cmd.pipelineBarrier2(&DI);
	    }
	  } else {
	    //allTargets.template ofLayout<RB.layoutId>();
	  }
	} else {
	  //TODO
	}
	doBarriers<RBS.sub(1)>();
      }
    };

    template<resourceBarrierTiming BT> inline void doBarriersForTime() {
      constexpr copyableArray<resourceBarrier, barriersForTime(BT).size()> BTS = barriersForTime(BT);
      doBarriers<BTS>();
    };

    template<literalList<copyStep> CS> inline void doCopies() {
      if constexpr(CS.len) {
	//TODO
	doCopies<CS.sub(1)>();
      }
    };

    template<size_t layerIdx> inline void renderFrom() {
      if constexpr(layerIdx < OD.LRS.len) {
	static constexpr layerRequirements LR = OD.LRS[layerIdx];
	renderFrom<layerIdx+1>();
      }
    };

    void render() {
      //TODO acquire mutx lock
      renderFrom<0>();
    }

  };

};
