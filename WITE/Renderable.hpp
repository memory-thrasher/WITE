#pragma once

#include <vector>

#include "Vulkan.hpp"
#include "ShaderData.hpp"
#include "types.hpp"
#include "WorkBatch.hpp"

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;
  template<acceptShaderData(D)> class Shader;

  class RenderableBase {
  private:
    PerGpu<Semaphore>* readDependency;
  protected:
    RenderableBase(layer_t layer) : layer(layer) {};
    virtual void bind(size_t gpu, WorkBatch cmd, vk::PipelineBindPoint bp) = 0;
  public:
    const layer_t layer;
    virtual void render(WorkBatch cmd) = 0;
    virtual ~RenderableBase() = default;
    void setReadDependency(PerGpu<Semaphore>* d) {
      readDependency = d;
    };
  };

  //renderable has a specific shader but what type it is is abstracted out.
  //Use the specific renderable implementation for every possible shader.
  template<acceptShaderData(D)> class Renderable : public RenderableBase {
  private:
    typedef Shader<passShaderData(D)> shader_t;
    shader_t shader;
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
    void bind(RenderTarget& target, WorkBatch cmd) {
      auto ds = data.get(gpu);
      #error TODO add a callback here to the creator to update the transform or w/e. Bind DBE to CArg onSpinUp
      cmd.bindDescriptorSets(shader->getBindPoint(), shader_t::getLayout(gpu), 2, 1, &ds, 0, NULL);
    };
    virtual ~Renderable() {
      shader->remove(this);
    };
  };

};
