//
//static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D16_UNORM, COLOR_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
//static constexpr VkAttachmentDescription attachments[2] =
//{{0, COLOR_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
//VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
//{0, DEPTH_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
//VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
//static constexpr VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
//depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
//static constexpr VkSubpassDescription subpass = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &colorReference, NULL, &depthReference, 0, NULL};
//static constexpr VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 2, attachments, 1, &subpass, 0, NULL};
//static constexpr VkClearValue CLEAR[2] = {{{{0.2f, 0.2f, 0.2f, 0.2f}}}, {{1.0, 0}}};
//static constexpr VkClearAttachment CLEAR_ATTACHMENTS[2] = {
//  {VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{0.5f, 0.5f, 0.5f, 0.5f}}}},
//{VK_IMAGE_ASPECT_DEPTH_BIT, 1, {{1.0, 0}}}
//};
//VkRenderPass rp;
//VkFramebuffer fb;//output of render (null if none or non-image)//TODO make this an optional component of BackedImage
//VkRenderPassBeginInfo beginInfo;

#include "Internal.h"

RenderPass::RenderPass(uint64_t initMode, uint64_t initLayout, uint64_t endLayout, Queue* queue) {
  //TODO this should be a convenience constructor for the bigger one that supports multiple buffers and subpasses
  attachmentCount = 2;
  bufferCount = 1;
  buffers = (buffer*)malloc(sizeof(buffer));
  buffers->attachments = (attachment*)malloc(sizeof(attachment) * attachmentCount);
  vkCreateRenderPass();//TODO
  beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, VK_NULL_HANDLE, rp, VK_NULL_HANDLE,
  {{0, 0}, {(uint32_t)size.width(), (uint32_t)size.height()}}, 2, RenderPass::CLEAR };
}

void RenderPass::setImages(size_t bufferCount, WITE::BackedImage* Wcolor, WITE::BackedImage* Wdepth, size_t subpass = 0, bool singleSharedDepth = true) {
  BackedImage* color = (BackedImage*)Wcolor, *depth = (BackedImage*)Wdepth;
  size_t width = color->getWidth(),
    height = color->getHeight();
#ifdef _DEBUG
  for(size_t i = 0;i < bufferCount;i++) {
    if(color[i].getWidth() != width) CRASH("illegal color image width (%d) on fb %d of subpass %d\n", color[i].getWidth(), i, subpass);
    if(color[i].getHeight() != height) CRASH("illegal color image height (%d) on fb %d of subpass %d\n", color[i].getHeight(), i, subpass);
    if(!singleSharedDepth && depth[i].getWidth() != width) CRASH("illegal depth image width (%d) on fb %d of subpass %d\n", depth[i].getWidth(), i, subpass);
    if(!singleSharedDepth && depth[i].getHeight() != height) CRASH("illegal depth image height (%d) on fb %d of subpass %d\n", depth[i].getHeight(), i, subpass);
  }
  if(singleSharedDepth) {
    if(depth->getWidth() != width) CRASH("illegal depth image width (%d) on fb of subpass %d\n", depth->getWidth(), subpass);
    if(depth->getHeight() != height) CRASH("illegal depth image height (%d) on fb of subpass %d\n", depth->getHeight(), subpass);
  }
#endif // DEBUG
  VkImageView* views = (VkImageView*)malloc(sizeof(VkImageView) * attachmentCount);
  if(singleSharedDepth)
    views[0] = depth->getImageView();
  VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, rp, attachmentCount, views, width, height, 1 };
  for(size_t i = 0;i < bufferCount;i++) {
    VkCreateFrameBuffer(queue->gpu->device, &fbInfo, NULL, buffers->fb);
  }
  free(views);
}

void RenderPass::begin(VkCommandBuffer cmd) {
  vkCmdBeginRenderPass(cmd, &beginInfo, )
}
