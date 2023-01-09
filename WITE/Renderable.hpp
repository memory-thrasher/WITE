#pragma once

#include <vector>

#include "Vulkan.hpp"
#include "ShaderData.hpp"
#include "types.hpp"
#include "ElasticCommandBuffer.hpp"

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;
  template<acceptShaderData(D)> class Shader;

  class RenderableBase {
  private:
    PerGpu<Semaphore>* readDependency;
  protected:
    RenderableBase(layer_t layer) : layer(layer) {};
    virtual void bind(size_t gpu, ElasticCommandBuffer& cmd, vk::PipelineBindPoint bp) = 0;
    virtual void renderImpl(ElasticCommandBuffer& cmd) = 0;
  public:
    const layer_t layer;
    void render(ElasticCommandBuffer& cmd) {
      if(readDependency)
	cmd.addDependency(readDependency->getPtr(cmd.getGpuIdx()));
      renderImpl(cmd);
    };
    virtual ~RenderableBase() = default;
    void setReadDependency(PerGpu<Semaphore>* d) {
      readDependency = d;
    };
  };

  //renderable has a specific shader but what type it is is abstracted out.
  //Use the specific renderable implementation for every possible shader.
  template<acceptShaderData(D)> class Renderable : public RenderableBase {
  private:
    Shader<passShaderData(D)>* shader;
    typedef ShaderDescriptor<passShaderData(D), ShaderResourceProvider::eRenderable> data_t;
    data_t data;
  protected:
    Renderable(layer_t layer, Shader<passShaderData(D)>* s) : RenderableBase(layer), shader(s) {
      s->add(this);
    };
  public:
    template<class... R> void setResources(R... resources) {
      data.setResources(std::forward<R...>(resources...));
    };
    void bind(size_t gpu, ElasticCommandBuffer& cmd) {
      auto ds = data.get(gpu);
      cmd->bindDescriptorSets(shader->getBindPoint(), 2, 1, &ds, 0, NULL);
    };
    virtual ~Renderable() {
      shader->remove(this);
    };
  };

};
