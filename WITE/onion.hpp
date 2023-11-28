#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"
#include "buffer.hpp"
#include "image.hpp"

namespace WITE {

  template<literalList<imageFlowStep> IFS, literalList<imageRequirements> IRS, literalList<bufferRequirements> BRS, literalList<shader> S, literalList<targetLayout> TLS, uint64_t ONION_ID, uint64_t GPUID> struct onion { // because layers

    template<resourceMap> struct mappedResourceTuple_t : std::false_type {};

    template<resourceMap RM> requires(containsId(IRS, RM.requirementId)) struct mappedResourceTuple_t<RM> {
      typedef image<IRS[findId(IRS, RM.requirementId)]> type;
    };

    template<resourceMap RM> requires(containsId(BRS, RM.requirementId)) struct mappedResourceTuple_t<RM> {
      typedef buffer<BRS[findId(BRS, RM.requirementId)]> type;
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
      static constexpr size_t TARGET_IDX = findId(TLS, TARGET_ID);
      static constexpr targetLayout TL = TLS[TARGET_IDX];

    private:
      mappedResourceTuple<TL.targetProvidedResources> resources;

      target_t(const target_t&) = delete;
      target_t() = default;

      friend onion;

    public:

      ~target_t() = default;

      template<uint64_t resourceMapId> void set(auto* v) {
	resources.template at<findId(TL.targetProvidedResources, resourceMapId)>() = v;
      };

      template<uint64_t resourceMapId> auto* get() {
	return resources.template at<findId(TL.targetProvidedResources, resourceMapId)>();
      };

    };

    template<uint64_t targetId> target_t<targetId> createTarget() {
      return {};
    };

    template<uint64_t SHADER_ID> class source_t {
    public:
      static constexpr size_t SHADER_IDX = findId(S, SHADER_ID);
      static constexpr shader SHADER_PARAMS = S[SHADER_IDX];

    private:
      mappedResourceTuple<SHADER_PARAMS.sourceProvidedResources> resources;

      friend onion;

    public:

      template<uint64_t resourceMapId> void set(auto* v) {
	resources.template at<findId(SHADER_PARAMS.sourceProvidedResources, resourceMapId)>() = v;
      };

      template<uint64_t resourceMapId> auto* get() {
	return resources.template at<findId(SHADER_PARAMS.sourceProvidedResources, resourceMapId)>();
      };

    };

    template<uint64_t shaderId> source_t<shaderId> createSource() {
      return {};
    };

  };

};
