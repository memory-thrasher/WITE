#pragma once

#include "templateStructs.hpp"
#include "onionUtils.hpp"

namespace WITE {

  template<literalList<shader> S, shaderTargetLayout TL, uint64_t ONION_ID, uint64_t GPUID = 0> struct onion {

    template<uint64_t id> inline static constexpr auto imageRequirements_v = getImageRequirements<id, ONION_ID, S, GPUID>();

    template<uint64_t id> inline static constexpr auto bufferRequirements_v = getBufferRequirements<id, TL, S, GPUID>();

  };

};
