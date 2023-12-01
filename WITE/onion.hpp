#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"

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

    template<literalList<resourceMap> RM> requires(RM.len == 0)
      struct mappedResourceTuple<RM> : std::false_type {};

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

    };

    template<uint64_t targetId> target_t<targetId> createTarget() {
      return { this };
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

    template<uint64_t sourceId> source_t<sourceId> createSource() {
      return { this };
    };

  };

};
