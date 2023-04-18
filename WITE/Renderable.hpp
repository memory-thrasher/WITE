#pragma once

#include <vector>

#include "Vulkan.hpp"
#include "ShaderData.hpp"
#include "types.hpp"
#include "WorkBatch.hpp"
#include "ShaderDescriptorLifeguard.hpp"

namespace WITE::GPU {

  class RenderTarget;
  class GpuResource;
  template<ShaderData D> class Shader;

  class RenderableBase {
  private:
    PerGpu<Semaphore>* readDependency;
  protected:
    RenderableBase(layer_t layer) : layer(layer) {};
    virtual void bind(RenderTarget& target, WorkBatch cmd) = 0;
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
  template<ShaderData D> class Renderable : public RenderableBase {
  private:
    typedef Shader<D> shader_t;
    shader_t* shader;
    typedef ShaderDescriptor<D, ShaderResourceProvider::eRenderable> data_t;
    data_t data;
  protected:
    Renderable(layer_t layer, Shader<D>* s) : RenderableBase(layer), shader(s) {
      s->add(this);
    };
  public:
    template<class... R> void setResources(R... resources) {
      data.setResources(std::forward<R...>(resources...));
    };
    virtual void bind(RenderTarget& target, WorkBatch cmd) override {
      size_t gpu = cmd.getGpuIdx();
      auto ds = data.get(gpu);
      cmd.bindDescriptorSets(shader->getBindPoint(), shader_t::getLayout(gpu), 2, 1, &ds, 0, NULL);
    };
    virtual ~Renderable() {
      shader->remove(this);
    };
  };

};
