#include "ShaderModule.hpp"
#include "Vulkan.hpp"

namespace WITE::GPU {

  template<ShaderData D> class VertexShader : public Shader<D> {
    static_assert(D.contains(ShaderData::ColorAttachment));
    static_assert(D.containsOnly({
	  ShaderStage::eDraw,
	  ShaderStage::eAssembler,
	  ShaderStage::eVertex,
	  ShaderStage::eTessellation,
	  ShaderStage::eGeometry,
	  ShaderStage::eFragmentDepth,
	  ShaderStage::eFragment,
	  ShaderStage::eBlending
	}));
  protected:
    ShaderModule* vertex, tessellationControl, tessellationEvaluation, geometry, fragment;//required: fragment
  public:
    vk::Pipeline CreatePipe(size_t idx) override;
    void RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) override;
    void BindImpl(ElasticCommandBuffer& cmd) override;
    VertexShader(ShaderModule* v, ShaderModule* tc, ShaderModule* te, ShaderModule* g, ShaderModule* f) :
      vertex(v), tessellationControl(tc), tessellationEvaluation(te), geometry(g), fragment(f)
    {
      ASSERT_TRAP(!v || v->has(vk::ShaderStageFlagBits::eVertex), "no entry point provided for that shader module");
      ASSERT_TRAP(!tc || tc->has(vk::ShaderStageFlagBits::eTessellationControl),
		  "no entry point provided for that shader module");
      ASSERT_TRAP(!te || te->has(vk::ShaderStageFlagBits::eTessellationEvaluation),
		  "no entry point provided for that shader module");
      ASSERT_TRAP(!g || g->has(vk::ShaderStageFlagBits::eGeometry), "no entry point provided for that shader module");
      ASSERT_TRAP(f && f->has(vk::ShaderStageFlagBits::eFragment), "no entry point provided for that shader module");
    };
    VertexShader(ShaderModule* v, ShaderModule* tc, ShaderModule* te, ShaderModule* f) : VertexShader(v, tc, te, NULL, f) {};
    VertexShader(ShaderModule* v, ShaderModule* g, ShaderModule* f) : VertexShader(v, NULL, NULL, g, f) {};
    VertexShader(ShaderModule* v, ShaderModule* f) : VertexShader(v, NULL, NULL, NULL, f) {};
    VertexShader(ShaderModule* f) : VertexShader(NULL, NULL, NULL, NULL, f) {};
  };

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
    static_assert(D.containsOnly({ ShaderStage::eCompute }));
    //TODO public constructor that asks for modules for every stage
  };

  vk::Pipeline VertexShader::CreatePipe(size_t idx) {
    Gpu& dev = Gpu::get(idx);
    auto vkDev = dev.getVkDevice();
    vk::Pipeline ret;
    vk::PipelineShaderStageCreateInfo stages[5];
    uint32_t stageCnt = 0;
    if(vertex) stages[stageCnt++] = vertex->getCI(vk::ShaderStageFlagBits::eVertex, idx);
    if(tessellationControl) stages[stageCnt++] = tessellationControl->getCI(vk::ShaderStageFlagBits::eTessellationControl, idx);
    if(tessellationEvaluation)
      stages[stageCnt++] = tessellationEvaluation->getCI(vk::ShaderStageFlagBits::eTessellationEvaluation, idx);
    if(geometry) stages[stageCnt++] = geometry->getCI(vk::ShaderStageFlagBits::eGeometry, idx);
    stages[stageCnt++] = fragment->getCI(vk::ShaderStageFlagBits::eFragment, idx);
    HPP_NAMESPACE::ArrayProxyNoTemporaries<const vk::PipelineShaderStageCreateInfo> packedStages(stageCnt, stages);
    GraphicsPipelineCreateInfo ci((vk::PipelineCreateFlags)0, &stages, );
    vkDev.createGraphicsPipeline(dev.getPipelineCache(), 1, ci, ALLOCCB, &ret);
    return ret;
  };

  void RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) override;

  void VertexShader::BindImpl(RenderTarget& target, ElasticCommandBuffer& cmd) {
    //TODO bind pipeline
    //TODO begin render pass
    //TODO bind globals
    //TODO bind each render target to matching resource type in order of match (only attachments are possible)
  }

};
