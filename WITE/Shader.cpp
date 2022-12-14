#include "Shader.hpp"
#include "Renderable.hpp"
#include "RenderTarget.hpp"
#include "GpuResource.hpp"
#include "Gpu.hpp"
#include "ElasticCommandBuffer.hpp"

namespace WITE::GPU {

  std::vector<ShaderBase*> ShaderBase::allShaders; //static
  std::atomic<pipelineId_t> ShaderBase::idSeed;
  PerGpu<std::map<ShaderData, vk::PipelineLayout>> ShaderBase::layoutsByData;
  Util::SyncLock ShaderBase::layoutsByData_mutex;

  ShaderBase::ShaderBase(QueueType qt) : queueType(qt)
  {
    allShaders.push_back(this);
  };

  ShaderBase::~ShaderBase() {
    Collections::remove(allShaders, this);
  };

  void ShaderBase::Register(Renderable* r) {
    renderablesByLayer[r->layer].push(r);
  };

  void ShaderBase::Unregister(Renderable* r) {
    renderablesByLayer[r->layer].remove(r);
  };

  void ShaderBase::RenderAllOfTypeTo(RenderTarget& target, ElasticCommandBuffer& cmd, QueueType qt, layerCollection_t& layers, std::initializer_list<Renderable*>& except) {// static
    for(ShaderBase* s : allShaders)
      if(s->queueType == qt)
	s->RenderTo(target, cmd, layers, except);
  };

  void ShaderBase::RenderTo(RenderTarget& target, ElasticCommandBuffer& cmd, layerCollection_t& layers, std::initializer_list<Renderable*>& except) {
    if(!layers.intersectsMap(renderablesByLayer)) return;
    bool found = false;
    for(auto& layer : layers)
      if(renderablesByLayer.contains(layer) && renderablesByLayer.at(layer).count()) {
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
    for(auto& pair : renderablesByLayer)
      if(layers.contains(pair.first))
	for(Renderable* renderable : pair.second)
	  if(!Collections::contains(except, renderable))
	    RenderImpl(renderable, cmd);
  };

  vk::PipelineLayout ShaderBase::getLayout(const ShaderData& d, vk::PipelineLayoutCreateInfo* pipeCI, size_t gpu) { //static
    Util::ScopeLock lock(&layoutsByData_mutex);
    if(!layoutsByData.contains(gpu, d))
      layoutsByData.get(gpu, d) = ShaderBase::makeLayout(pipeCI, gpu);
    return layoutsByData.get(gpu, d);
  };

  vk::PipelineLayout ShaderBase::makeLayout(const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx) { //static
    vk::PipelineLayout ret;
    auto vkDev = Gpu::get(gpuIdx).getVkDevice();
    VK_ASSERT(vkDev.createPipelineLayout(pipeCI, ALLOCCB, &ret), "failed pipeline layout");
    return ret;
  };

  vk::Format VertexAspectSpecifier::getFormat() const { return formatsBySizeTypeQty.at(*this); };

};
