#include "Shader.hpp"
#include "Renderer.hpp"
#include "GpuResource.hpp"
#include "Gpu.hpp"
#include "ElasticCommandBuffer.hpp"

namespace WITE::GPU {

  std::vector<ShaderBase*> ShaderBase::allShaders; //static

  ShaderBase::ShaderBase(QueueType qt) :
    pipeline(decltype(pipeline)::creator_t_F::make(this, &ShaderBase::CreatePipe),//might need proxy method, createpip is virtual
	     decltype(pipeline)::creator_t_F::make(this, &ShaderBase::DestroyPipe)),
    queueType(qt)
  {
    allShaders.push_back(this);
  };

  ShaderBase::~ShaderBase() {
    Collections::remove(allShaders, this);
  };

  void ShaderBase::Register(Renderer* r) {
    renderersByLayer.[r->layer].push(r);
  };

  void ShaderBase::Unregister(Renderer* r) {
    renderersByLayer.[r->layer].remove(r);
  };

  void ShaderBase::RenderAllTo(RenderTarget& target, const layerCollection_t& layers,
			       std::initializer_list<Renderer*> except) {// static
    //TODO make target.ldm exist
    auto batch = Gpu.getGpuFor(target.ldm).getQueue(queueType).createBatch(target.ldm);
    for(ShaderBase* s : allShaders)
      s->RenderTo(target, layers, batch, except);
  };

  void ShaderBase::RenderTo(RenderTarget& target, const layerCollection_t& layers, ElasticCommandBuffer& cmd,
			    std::initializer_list<Renderer*> except) {
    if(!layers.intersectsMap(renderersByLayer.getRead())) return;
    bool found = false;
    for(auto pair : renderersByLayer)
      if(layers.contains(pair.first) && pair.second.count()) {
	found = true;
	break;
      }
    if(!found) return;
    BindImpl(target, cmd);
    for(auto pair : renderersByLayer)
      if(layers.contains(pair.first))
	for(Renderer* renderer : pair.second)
	  if(!Collections::contains(except.begin(), except.end(), renderer))
	    RenderImpl(renderer, cmd);
    target.dirtyOutputs();
  };

  void DestroyPipe(vk::Pipeline pipe, size_t idx) {
    GPU::get(idx).getVkDevice().destroyPipeline(pipe, ALLOCCB);
  };

};
