#include "ShaderModule.hpp"
#include "Vulkan.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  template<ShaderData D, VertexModel VM> class VertexShader : public Shader<D> {
  public:
    typedef Vertex<VM> vertex_t;
    typedef VertexShader<D, VM> S;
  private:
    static_assert(D.contains(ShaderData::DepthAttachment));
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
    constexpr static uint32_t descriptorSetResourceCount = D.countDescriptorSetResources();
    constexpr static vk::DescriptorSetLayoutBinding dsBindings[descriptorSetResourceCount] =
		D.getDescriptorSetLayoutBindings<descriptorSetResourceCount>();
    constexpr static vk::PipelineLayoutCreateInfo pipeCI { {}, descriptorSetResourceCount, dsBindings };//defaulted no push constants
    vk::VertexInputBindingDescription viBinding { 0, sizeof(vertex_t), vk::VertexInputRate::eVertex };
    vk::VertexInputAttributeDescription viAttributes[VM.count()];
    vk::PipelineVertexInputStateCreateInfo vi { (vk::PipelineVertexInputStateCreateFlags)0,
      1, &viBinding, VM.count(), &viAttributes };
    vk::PipelineInputAssemblyStateCreateInfo ias { {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE };
    vk::PipelineTessellationStateCreateInfo ts { {}, 0 };
    vk::PipelineViewportStateCreateInfo vs { {}, 1, NULL, 1, NULL, NULL };
    vk::PipelineRasterizationStateCreateInfo rs { {}, false, true, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
      vk::FrontFace::eCounterClockwise };//depth bias left to default disabled
    vk::PipelineMultisampleStateCreateInfo ms;//default = single-sample
    vk::PipelineDepthStencilStateCreateInfo dss { {}, true, true, vk::CompareOp::eLess, false, true,
						  { vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep,
						    vk::CompareOp::eLess } };//skipped back and depth boudns checking (for now)
    const vk::PipelineColorBlendAttachmentState defaultBlendAtt { false };//no blend
    vk::PipelineColorBlendStateCreateInfo cbs { {}, false, vk::LogicOp::eClear, 1, &defaultBlendAtt };//default blend constants
    vk::DynamicState dse[2] { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo ds { {}, sizeof(dse) / sizeof(vk::DynamicState), dse };
    vk::GraphicsPipelineCreateInfo ci { (vk::PipelineCreateFlags)0, 0, {}, &vi, &ias, &ts, &vs, &rs, &ms, &dss,
      &cbs, &ds, {}, {}, 0, {}, 0 };//layout and RP are provided at runtime
  protected:
    ShaderModule* vertex, *tessellationControl, *tessellationEvaluation, *geometry, *fragment;//required: fragment
  public:
    vk::Pipeline CreatePipe(size_t idx, vk::RenderPass rp) override;
    void RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) override;
    vk::PipelineBindPoint getBindPoint() override;

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
      ASSERT_TRAP(tessellationEvaluation == NULL && tessellationControl == NULL, "NYI tessellation");//temp
      for(size_t i = 0, j = 0;i < VM.count();i++) {
	viAttributes[i] = { i, 0, VM[i].getFormat(), j };
	j += sizeof(VertexAspect_t<VM[i]>);
      }
    };

    VertexShader(ShaderModule* v, ShaderModule* tc, ShaderModule* te, ShaderModule* f) : VertexShader(v, tc, te, NULL, f) {};
    VertexShader(ShaderModule* v, ShaderModule* g, ShaderModule* f) : VertexShader(v, NULL, NULL, g, f) {};
    VertexShader(ShaderModule* v, ShaderModule* f) : VertexShader(v, NULL, NULL, NULL, f) {};
    VertexShader(ShaderModule* f) : VertexShader(NULL, NULL, NULL, NULL, f) {};
    //TODO factory that just grabs whatever stages a single module has
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

  //TODO raytracing

  template<ShaderData D, VertexModel VM> vk::Pipeline VertexShader<D, VM>::CreatePipe(size_t idx, vk::RenderPass rp) {
    Gpu& dev = Gpu::get(idx);
    auto vkDev = dev.getVkDevice();
    vk::Pipeline ret;
    vk::GraphicsPipelineCreateInfo gpci = ci;//copy
    vk::PipelineShaderStageCreateInfo stageCIs[5];
    uint32_t stageCnt = 0;
    if(vertex) stageCIs[stageCnt++] = vertex->getCI(vk::ShaderStageFlagBits::eVertex, idx);
    if(tessellationControl) stageCIs[stageCnt++] = tessellationControl->getCI(vk::ShaderStageFlagBits::eTessellationControl, idx);
    if(tessellationEvaluation)
      stageCIs[stageCnt++] = tessellationEvaluation->getCI(vk::ShaderStageFlagBits::eTessellationEvaluation, idx);
    if(geometry) stageCIs[stageCnt++] = geometry->getCI(vk::ShaderStageFlagBits::eGeometry, idx);
    stageCIs[stageCnt++] = fragment->getCI(vk::ShaderStageFlagBits::eFragment, idx);
    gpci.layout = ShaderBase::getLayout(D, &pipeCI, idx);
    gpci.renderPass = rp;
    gpci.pStages = stageCIs;
    VK_ASSERT_TUPLE(ret, vkDev.createGraphicsPipeline(dev.getPipelineCache(), ci, ALLOCCB), "Failed to create pipe");
    return ret;
  };

  template<ShaderData D, VertexModel VM> void VertexShader<D, VM>::RenderImpl(Renderer* r, ElasticCommandBuffer& cmd) {
    //TODO
  };

  template<ShaderData D, VertexModel VM> vk::PipelineBindPoint VertexShader<D, VM>::getBindPoint() {
    return vk::PipelineBindPoint::eGraphics;
  };

};

