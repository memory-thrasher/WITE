#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"

namespace WITE {

  template<LiteralList<shader> S, shaderTargetLayout TL, uint64_t ONION_ID, uint64_t GPUID = 0> struct onion {

    template<uint64_t id> struct imageRequirements_vt {
      static constexpr auto value = getImageRequirements<id, ONION_ID, S, GPUID>();
    };
    template<uint64_t id> inline constexpr auto imageRequirements_v = imageRequirements_vt<id>::value;

  };

};
