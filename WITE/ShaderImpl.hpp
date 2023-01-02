#include "VertexShader.hpp"

namespace WITE::GPU {

  //TODO move these to their own files
  template<acceptShaderData(D)> class MeshShader : public Shader<passShaderData(D)> {
    static_assert(ParseShaderData<passShaderData(D)>().containsOnly({
	  ShaderStage::eTask,
	  ShaderStage::eMesh,
	  ShaderStage::eFragmentDepth,
	  ShaderStage::eFragment,
	  ShaderStage::eBlending
	}));
    //TODO public constructor that asks for modules for every stage
  };

  template<acceptShaderData(D)> class ComputeShader : public Shader<passShaderData(D)> {
    static_assert(ParseShaderData<passShaderData(D)>().containsOnly({ ShaderStage::eCompute }));
    //TODO public constructor that asks for modules for every stage
  };

  //TODO raytracing

};

