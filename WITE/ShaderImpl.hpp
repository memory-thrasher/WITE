#include "VertexShader.hpp"

namespace WITE::GPU {

  //TODO move these to their own files
  template<ShaderData D> class MeshShader : public Shader<D> {
    static_assert(D.containsOnly({
	  ShaderStage::eTask,
	  ShaderStage::eMesh,
	  ShaderStage::eFragmentDepth,
	  ShaderStage::eFragment,
	  ShaderStage::eBlending
	}));
    //TODO public constructor that asks for modules for every stage
  };

  template<ShaderData D> class ComputeShader : public Shader<D> {
    static_assert(ParseShaderData<D>().containsOnly({ ShaderStage::eCompute }));
    //TODO public constructor that asks for modules for every stage
  };

  //TODO raytracing

};

