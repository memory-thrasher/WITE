#pragma once

#include "constants.hpp"
#include "LiteralList.hpp"

namespace WITE {

  typedef uint64_t layer_t;

  namespace GPU {

    typedef const Collections::LiteralList<layer_t> layerCollection_t;

    enum class QueueType { eGraphics, eTransfer, eCompute };

  }

  template<size_t levels> struct uint_auto_base {};
  template<> struct uint_auto_base<0> { typedef uint_fast8_t type; };
  template<> struct uint_auto_base<1> { typedef uint_fast16_t type; };
  template<> struct uint_auto_base<2> { typedef uint_fast32_t type; };
  template<> struct uint_auto_base<3> { typedef uint_fast64_t type; };
  template<size_t bits> using uint_auto = typename uint_auto_base<(bits > 8) + (bits > 16) + (bits > 32)>::type;
  using deviceMask_t = uint_auto<MAX_GPUS>;
  using logicalDeviceMask_t = uint_auto<MAX_LDMS>;

  using usage_t = uint64_t;
  using pipelineId_t = uint64_t;

}
