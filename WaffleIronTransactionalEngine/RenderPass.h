#pragma once

class RenderPass : public WITE::RenderPass {
public:
  RenderPass(uint64_t initMode, uint64_t initLayout, uint64_t endLayout, Queue* queue);//ordinary default: one color, one depth, one subpass
  //TODO multi-image per-image type, per image load op (ternery), per image pre and post layout
  //TODO detailed constructor
  //RenderPass(VkSampleCountFlagBits, store op, )
  //TODO non-graphics passes, if that's even a thing
  //TODO multi-sub-pass
  void begin(VkCommandBuffer);
  void setClearColor(glm::vec4 color, size_t imageIdx = -1);//color in bgra order, imageIdx optional if only one image
  void setClearDepth(float depth, uint32_t stencil, size_t imageIdx = -1);
  void setImages(size_t bufferCount, WITE::BackedImage* color, WITE::BackedImage* depth, size_t subpass = 0, bool singleSharedDepth = true);//TODO need subpass and attachment indecies
private:
  typedef struct {
    bool isDepth;
    BackedImage* image;
    uint64_t initMode, initLayout, endLayout;
    VkClearValue clearTo;
  } attachment;
  typedef struct {
    attachment* attachments;
    VkFramebuffer fb;
  } buffer;
  buffer* buffers;
  size_t attachmentCount;
  size_t bufferCount;
  VkClearValue* clear;
  Queue* queue;
  VkRenderPassBeginInfo beginInfo;
  VkRenderPass rp;
};
