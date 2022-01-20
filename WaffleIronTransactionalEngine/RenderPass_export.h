#pragma once

namespace WITE {
  
  class RenderPass : public WITE::ImageSource {
  public:
    static std::shared_ptr<RenderPass> make(WITE::Queue*);
    static std::shared_ptr<RenderPass> make(WITE::Queue*, WITE::ImageSource **inputs, size_t inputCount);
    RenderPass(const RenderPass&) = delete;
    virtual void setClearColor(glm::vec4 color, size_t imageIdx = -1) = 0;
    virtual void setClearDepth(float depth, uint32_t stencil, size_t imageIdx = -1) = 0;
    virtual bool appliesOnLayer(WITE::renderLayerIdx i) = 0;
    virtual void setLayermask(WITE::renderLayerMask newMask) = 0;
    virtual void render();
    virtual void requestResize(IntBox3D) = 0;
  protected:
    RenderPass() = default;
    RenderPass(RenderPass&) = delete;
  };

};
