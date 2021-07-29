#pragma once

/*
  For now, each RP consists of exactly one color output, one depth output, and 0 or more readonly input images
 */
class RenderPass : public WITE::RenderPass {
public:
  // typedef struct Attachment_t {
  //   BackedImage* image;
  //   uint64_t initMode, endLayout, sizeType;
  //   VkClearValue clearTo;//color in bgra order
  //   //TODO multi-image per-image type, per image load op (ternery), per image pre (?) and post layout
  // } Attachment;
  //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>MAKE THE IMAGE source OWN THE RENDERPASS!!!!!!!<<<<<<<<<<<<<<<<<<<<<<<<<<
  /*have an abstract source, like meshes.
    Pass an image source to each camera on creation
    image source has a function to report a viewport size change
    camera just blits it's image, and percolates a size change
    windows should notify cameras of size change based on window event, not just failed present
    add render callback to object functions, supply camera array (need world camera array, store in camera)
    update on fixed image returns from buffer
    image from RP does render on update! Both need cmd anyway
   */
  RenderPass(WITE::Queue*);
  RenderPass(WITE::Queue*, WITE::ImageSource **inputs, size_t inputCount);
  void render();
  void setOutputSize(WITE::IntBox3D screensize);
  BackedImage* getImage() { return getColorOutputImage(); };//override from interface (TODO make interface)
  int64_t getFormat() { return FORMAT_STD_COLOR; };
  inline bool appliesOnLayer(WITE::renderLayerIdx i) { return layerMask & (static_cast<WITE::renderLayerIdx>(1) << i); };
  inline void setLayermask(WITE::renderLayerMask newMask) { layerMask = newMask; };
  inline BackedImage* getColorOutputImage() { return colorOutImage.get(); };
  inline BackedImage* getDepthOutputImage() { return depthOutImage.get(); };
private:
  Queue* queue;
  WITE::ImageSource* inputs;
  uint64_t fbResourceSerial;
  WITE::IntBox3D outputSize;
  size_t inputCount;
  VkFramebuffer fb;
  WITE::renderLayerMask layerMask;
  std::unique_ptr<VkAttachmentDescription[]> attachments;
  static constexpr VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  std::unique_ptr<VkAttachmentReference[]> inputReferences;
  static constexpr VkClearValue CLEAR[2] = {{{{0.2f, 0.2f, 0.2f, 0.2f}}}, {{1.0, 0}}};
  static constexpr VkClearAttachment CLEAR_ATTACHMENTS[2] = { {VK_IMAGE_ASPECT_COLOR_BIT, 0, {CLEAR[0]}}, {VK_IMAGE_ASPECT_DEPTH_BIT, 1, CLEAR[1]} };
  VkSubpassDescription subpass;
  VkRenderPassCreateInfo rpInfo;
  VkRenderPassBeginInfo beginInfo;
  VkRenderPass rp;
  VkFramebuffer fb;
  VkFramebufferCreateInfo fbInfo;
  VkImageView attachmentViews[];//array, for handing to fb creation. each is disposed by the BackedImage container
  std::unique_ptr<BackedImage> colorOutImage, depthOutImage;
};
