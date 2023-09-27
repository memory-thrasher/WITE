#include "UDM.hpp"

namespace WITE::Export {

  enum class usage_t : uint8_t {
    indirectBuffer,
    indexBuffer,
    vertexBuffer,
    uniformBuffer,
    storageBuffer,
    pushConstant,
    sampledImage,
    storageImage,
    depthStencilAttachment, //maybe split into d/s/d+s ?
    inputAttachment,
    colorAttachment
  };

  struct layerLayoutDatum {
    uint32_t resourceIdx;
    usageType_t type;
    udm attributes; //vertex buffer, may eventually be useful for other options also
  };

  typedef LiteralList<layerLayoutDatum> LayerLayout;

}
