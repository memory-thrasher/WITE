#pragma once

#include "StructuralConstList.hpp"

namespace WITE {

  typedef uint64_t layer_t;

  namespace GPU {

    typedef Collections::StructuralConstList<layer_t> layerCollection_t;

    enum class QueueType { eGraphics, eTransfer, eCompute };

  }

}
