#pragma once

namespace WITE {
  
  class RenderPass {
  public:
    void setClearColor(glm::vec4 color, size_t imageIdx = -1);//color in bgra order, imageIdx optional if only one image
    void setClearDepth(float depth, uint32_t stencil, size_t imageIdx = -1);
    static RenderPass* makeBasic(uint64_t initMode = INIT_MODE_CLEAR, uint64_t initLayout = LAYOUT_UNDEFINED, uint64_t endLayout = LAYOUT_TRANSFER_SRC);
    //TODO others
    virtual bool appliesOnLayer(WITE::renderLayerIdx i) = 0;
    virtual void setLayermask(WITE::renderLayerMask newMask) = 0;
  protected:
    RenderPass() = default;
    RenderPass(RenderPass&) = delete;
  };

};
