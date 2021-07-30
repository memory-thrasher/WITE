#pragma once

namespace WITE {
  
  class RenderPass : public WITE::ImageSource {
  public:
    static std::shared_ptr<RenderPass> make(WITE::Queue*);
    static std::shared_ptr<RenderPass> make(WITE::Queue*, WITE::ImageSource **inputs, size_t inputCount);
    RenderPass(const RenderPass&) = delete;
    virtual void setClearColor(glm::vec4 color, size_t imageIdx = -1) = 0;
    virtual void setClearDepth(float depth, uint32_t stencil, size_t imageIdx = -1) = 0;
    //static RenderPass* makeBasic(uint64_t initMode = INIT_MODE_CLEAR, uint64_t initLayout = LAYOUT_UNDEFINED, uint64_t endLayout = LAYOUT_TRANSFER_SRC);
    //TODO others
    virtual bool appliesOnLayer(WITE::renderLayerIdx i) = 0;
    virtual void setLayermask(WITE::renderLayerMask newMask) = 0;
    virtual void doRender();
  protected:
    RenderPass() = default;
    RenderPass(RenderPass&) = delete;
  };

};
