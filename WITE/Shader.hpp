#include "ShaderData.hpp"

namespace WITE::Gpu {

  class ShaderBase {
  };

  template<ShaderData D>
  class Shader : ShaderBase {
  };

  template<ShaderData D>
  class VertexShader : Shader<D> {
    static_assert(D.contains(ShaderData::ShaderResourceUsage::ColorAttachment));
  }:

  template<class D>
  class MeshShader : Shader<D> {
  }:

  template<class D>
  class ComputeShader : Shader<D> {
  }:

}
