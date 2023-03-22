#include "Shader.hpp"
#include "Renderable.hpp"
#include "RenderTarget.hpp"
#include "GpuResource.hpp"
#include "Gpu.hpp"

namespace WITE::GPU {

  std::vector<ShaderBase*> ShaderBase::allShaders; //static
  std::atomic<pipelineId_t> ShaderBase::idSeed;
  PerGpu<std::map<ShaderData::hashcode_t, vk::PipelineLayout>> ShaderBase::layoutsByData;
  Util::SyncLock ShaderBase::layoutsByData_mutex;

  ShaderBase::ShaderBase(QueueType qt) : queueType(qt)
  {
    allShaders.push_back(this);
  };

  ShaderBase::~ShaderBase() {
    Collections::remove(allShaders, this);
  };

  void ShaderBase::add(RenderableBase* r) {
    renderablesByLayer[r->layer].push(r);
  };

  void ShaderBase::remove(RenderableBase* r) {
    renderablesByLayer[r->layer].remove(r);
  };

  void ShaderBase::renderAllOfTypeTo(RenderTarget& target, WorkBatch cmd, QueueType qt, layerCollection_t& layers, std::initializer_list<RenderableBase*>& except) {// static
    for(ShaderBase* s : allShaders)
      if(s->queueType == qt)
	s->renderTo(target, cmd, layers, except);
  };

  void ShaderBase::renderTo(RenderTarget& target, WorkBatch cmd, layerCollection_t& layers, std::initializer_list<RenderableBase*>& except) {
    if(!Collections::collectionIntersectsMap(layers, renderablesByLayer)) return;
    bool found = false;
    for(auto& layer : layers)
      if(renderablesByLayer.contains(layer) && renderablesByLayer.at(layer).count()) {
	found = true;
	break;
      }
    if(!found) return;
    auto gpuIdx = cmd.getGpu().getIndex();
    vk::Pipeline pipe;
    if(!target.pipelinesByShaderId.contains(gpuIdx, id))
      pipe = target.pipelinesByShaderId.get(gpuIdx, id) = createPipe(gpuIdx, target.renderPasses.get(gpuIdx));
    else
      pipe = target.pipelinesByShaderId.get(gpuIdx, id);
    cmd.bindPipeline(getBindPoint(), pipe);
    preRender(target, cmd, layers, gpuIdx);
    for(auto& pair : renderablesByLayer)
      if(Collections::contains(layers, pair.first))
	for(RenderableBase* renderable : pair.second)
	  if(!Collections::contains(except, renderable))
	    renderable->render(cmd);
  };

  bool ShaderBase::hasLayout(ShaderData::hashcode_t d, size_t gpuIdx) { //static
    Util::ScopeLock lock(&layoutsByData_mutex);
    return layoutsByData.contains(gpuIdx, d);
  };

  vk::PipelineLayout ShaderBase::getLayout(ShaderData::hashcode_t d, size_t gpu) { //static
    Util::ScopeLock lock(&layoutsByData_mutex);
    return layoutsByData.get(gpu, d);
  };

  vk::PipelineLayout ShaderBase::makeLayout(ShaderData::hashcode_t d, const vk::PipelineLayoutCreateInfo* pipeCI, size_t gpuIdx) { //static
    vk::PipelineLayout ret;
    auto vkDev = Gpu::get(gpuIdx).getVkDevice();
    VK_ASSERT(vkDev.createPipelineLayout(pipeCI, ALLOCCB, &ret), "failed pipeline layout");
    {
      Util::ScopeLock lock(&layoutsByData_mutex);
      layoutsByData.get(gpuIdx, d) = ret;
    }
    return ret;
  };

  constexpr vk::Format VertexAspectSpecifier::getFormat() const { return formatsBySizeTypeQty.at(*this); };

};
