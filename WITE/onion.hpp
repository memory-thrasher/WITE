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

    template<resourceMap RM, uint64_t layoutId> consteval std::vector<resourceUsage> findUsages() {
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
	//rendering:
	for(size_t substepIdx : layer.renders) {
	  const auto& substep = findById(OD.RPRS, substepIdx);
	  if(RM.resourceReferences.contains(substep.depthStencil.id))
	    ret.push_back({ layerIdx, substep_e::render, substep.depthStencil });
	  if(RM.resourceReferences.contains(substep.color.id))
	    ret.push_back({ layerIdx, substep_e::render, substep.color });
	  std::map<uint8_t, resourceReference> urr;
	  for(const graphicsShaderRequirements& gsr : substep.shaders) {
	    for(const resourceReference& srr : gsr.targetProvidedResources)
	      if(RM.resourceReferences.contains(srr.id))
		urr[srr.frameLatency] |= srr;
	    for(const resourceReference& srr : gsr.sourceProvidedResources)
	      if(RM.resourceReferences.contains(srr.id))
		urr[srr.frameLatency] |= srr;
	  }
	  for(const auto& pair : urr) {
	    ret.push_back({ layerIdx, substep_e::render, pair.second });
	  }
	}
	//compute:
	std::map<uint8_t, resourceReference> urr;
	for(size_t csrId : layer.computeShaders) {
	  const computeShaderRequirements& csr = findById(OD.CSRS, csrId);
	  for(const resourceReference& srr : csr.targetProvidedResources)
	    if(RM.resourceReferences.contains(srr.id))
	      urr[srr.frameLatency] |= srr;
	  for(const resourceReference& srr : csr.sourceProvidedResources)
	    if(RM.resourceReferences.contains(srr.id))
	      urr[srr.frameLatency] |= srr;
	}
	for(const auto& pair : urr) {
	  ret.push_back({ layerIdx, substep_e::render, pair.second });
	}
      }
      std::sort(ret.begin(), ret.end());
      return ret;
    };

    template<resourceMap RM, uint64_t layoutId> consteval std::map<resourceBarrierTiming, std::vector<resourceBarrier>> findBarriers() {
      auto usage = findUsages<RM, layoutId>();
      size_t afterIdx = 0, beforeIdx = usage.size()-1;
      std::map<resourceBarrierTiming, std::vector<resourceBarrier>> ret;
      if(usage.size() < 2) return ret;
      while(afterIdx < usage.size()) {
	resourceBarrier rb;
	resourceBarrierTiming rbt;
	rb.before = usage[beforeIdx];
	rb.after = usage[afterIdx];
	rb.resourceId = RM.id;
	rb.layoutId = layoutId;
	constexpr_assert(rb.before < rb.after || afterIdx == 0);
	rbt.layerIdx = rb.after.layerIdx;
	rb.frameLatency = rb.after.usage.frameLatency;
	if(rb.before.layerIdx != rb.after.layerIdx) {//prefer top of layer for barriers
	  rbt.substep = substep_e::barrier1;
	} else {
	  constexpr_assert(rb.after.substep != rb.before.substep);
	  rbt.substep = (substep_e)((int)rb.after.substep - 1);//otherwise wait as long as possible
	}
	ret[rbt].push_back(rb);
	afterIdx++;
	beforeIdx = afterIdx-1;
      }
      return ret;
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
      renderFrom<0>();
    }

  };

};
