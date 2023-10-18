#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"

namespace WITE {

  template<literalList<shader> S, shaderTargetLayout TL, uint64_t ONION_ID, uint64_t GPUID = 0> struct onion {

    template<uint64_t id> struct imageRequirements_vt {
      static constexpr imageFlow<getImageFlowSize<id, ONION_ID, S>()> flow = getImageFlow(id, ONION_ID, S);
      static constexpr imageRequirements value = getImageRequirements<id, S, GPUID>(flow.flow);
    };
    template<uint64_t id> inline static constexpr auto imageRequirements_v = imageRequirements_vt<id>::value;
    template<uint64_t id> inline static constexpr auto imageFlow_v = imageRequirements_vt<id>::flow;

  };

};
