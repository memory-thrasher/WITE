#pragma once

namespace WITE {
  
  class RenderPass {
  public:
    void setClearColor(glm::vec4 color, size_t imageIdx = -1);//color in bgra order, imageIdx optional if only one image
    void setClearDepth(float depth, uint32_t stencil, size_t imageIdx = -1);
    static RenderPass* make(uint64_t initMode = INIT_MODE_CLEAR, uint64_t initLayout = LAYOUT_UNDEFINED, uint64_t endLayout = LAYOUT_TRANSFER_SRC);
    //TODO others
  protected:
    RenderPass() = default;
    RenderPass(RenderPass&) = delete;
  };

};
