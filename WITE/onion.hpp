#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"

namespace WITE {

  template<onionDescriptor OD> struct onion {

    // #error TODO internal frame counter

    template<resourceMap> struct mappedResourceTuple_t : std::false_type {};

    template<resourceMap RM> requires(containsId(OD.IRS, RM.requirementId)) struct mappedResourceTuple_t<RM> {
      typedef image<OD.IRS[findId(OD.IRS, RM.requirementId)]> type;
    };

    template<resourceMap RM> requires(containsId(OD.BRS, RM.requirementId)) struct mappedResourceTuple_t<RM> {
      typedef buffer<OD.BRS[findId(OD.BRS, RM.requirementId)]> type;
    };

    template<literalList<resourceMap> RM>
    struct mappedResourceTuple {
      mappedResourceTuple_t<RM[0]>::type data;
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
      mappedResourceTuple<TL.targetProvidedResources> resources;

      target_t(const target_t&) = delete;
      target_t() = default;

      friend onion;

    public:

      ~target_t() = default;

      // template<uint64_t resourceMapId> void set(auto* v) {
      // 	resources.template at<findId(TL.targetProvidedResources, resourceMapId)>() = v;
      // };

      template<uint64_t resourceMapId> auto* get() {
	return resources.template at<findId(TL.targetProvidedResources, resourceMapId)>();
      };

    };

    template<uint64_t targetId> target_t<targetId> createTarget() {
      return {};
    };

    template<uint64_t SOURCE_ID> class source_t {
    public:
      static constexpr size_t SOURCE_IDX = findId(OD.SLS, SOURCE_ID);
      static constexpr sourceLayout SOURCE_PARAMS = OD.SLS[SOURCE_IDX];

    private:
      mappedResourceTuple<SOURCE_PARAMS.sourceProvidedResources> resources;

      friend onion;

    public:

      // template<uint64_t resourceMapId> void set(auto* v) {
      // 	resources.template at<findId(SOURCE_PARAMS.sourceProvidedResources, resourceMapId)>() = v;
      // };

      template<uint64_t resourceMapId> auto* get() {
	return resources.template at<findId(SOURCE_PARAMS.sourceProvidedResources, resourceMapId)>();
      };

    };

    template<uint64_t sourceId> source_t<sourceId> createSource() {
      return {};
    };

  };

};
