#pragma once

namespace WITE {

  class export_def Renderer {//these are created by objects
  public:
    typedefCB(packDataCB, void, Renderer*, std::shared_ptr<WITE::ShaderResource>*, WITE::GPU*)//array of shared pointers, one per descriptor
      static void bind(Object* o, Shader* s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx);
    static void unbind(Object*, WITE::renderLayerIdx);
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    virtual void setOnceCallback(packDataCB) = 0;
    virtual void setPerFrameCallback(packDataCB) = 0;
    virtual void updateInstanceData(size_t resource, GPU* gpu) = 0;
    virtual Object* getObj() = 0;
    virtual Shader* getShader() = 0;
    virtual std::shared_ptr<Mesh> getMesh() = 0;
    //virtual RenderLayer* getLayer() = 0;//maybe just return WITE::renderLayerIdx
    inline bool exists() { return bool(getObj()); };
  protected:
    Renderer() = default;
  };

}
