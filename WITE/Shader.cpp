#include "Shader.hpp"
#include "Renderer.hpp"
#include "RenderTarget.hpp"
#include "GpuResource.hpp"
#include "Gpu.hpp"
#include "ElasticCommandBuffer.hpp"

namespace WITE::GPU {

  std::vector<ShaderBase*> ShaderBase::allShaders; //static

  ShaderBase::ShaderBase(QueueType qt) : queueType(qt)
  {
    allShaders.push_back(this);
  };

  ShaderBase::~ShaderBase() {
    Collections::remove(allShaders, this);
  };

  void ShaderBase::Register(Renderer* r) {
    renderersByLayer[r->layer].push(r);
  };

  void ShaderBase::Unregister(Renderer* r) {
    renderersByLayer[r->layer].remove(r);
  };

  void ShaderBase::RenderAllOfTypeTo(RenderTarget& target, ElasticCommandBuffer& cmd, QueueType qt, layerCollection_t& layers, std::initializer_list<Renderer*>& except) {// static
    for(ShaderBase* s : allShaders)
      if(s->queueType == qt)
	s->RenderTo(target, cmd, layers, except);
  };

  void ShaderBase::RenderTo(RenderTarget& target, ElasticCommandBuffer& cmd, layerCollection_t& layers, std::initializer_list<Renderer*>& except) {
    if(!layers.intersectsMap(renderersByLayer)) return;
    bool found = false;
    for(auto& layer : layers)
      if(renderersByLayer.contains(layer) && renderersByLayer.at(layer).count()) {
	found = true;
	break;
      }
    if(!found) return;
    auto gpuIdx = cmd.getGpu()->getIndex();
    vk::Pipeline pipe;
    if(!target.pipelinesByShaderId.contains(gpuIdx, id))
      pipe = target.pipelinesByShaderId.get(gpuIdx, id) = CreatePipe(gpuIdx, target.renderPasses.get(gpuIdx));
    else
      pipe = target.pipelinesByShaderId.get(gpuIdx, id);
    cmd->bindPipeline(getBindPoint(), pipe);
    for(auto& pair : renderersByLayer)
      if(layers.contains(pair.first))
	for(Renderer* renderer : pair.second)
	  if(!Collections::contains(except, renderer))
	    RenderImpl(renderer, cmd);
  };

  PerGpu<std::map<ShaderData, vk::PipelineLayout>> ShaderBase::layoutsByData;

  vk::PipelineLayout ShaderBase::getLayout(const ShaderData& d, const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpu) { //static
    Util::ScopeLock lock(&layoutsByData_mutex);
    if(!layoutsByData.contains(gpu, d))
      layoutsByData.get(gpu, d) = ShaderBase::makeLayout(pipeCI, gpu);
    return layoutsByData.get(gpu, d);
  };

  vk::PipelineLayout ShaderBase::makeLayout(const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx) { //static
    vk::PipelineLayout ret;
    VK_ASSERT(Gpu::get(gpuIdx).getVkDevice().createPipelineLayout(pipeCI, ALLOCCB, &ret), "failed pipeline layout");
    return ret;
  };

  vk::Format VertexAspectSpecifier::getFormat() const { return formatsBySizeTypeQty.at(*this); };

};
