#pragma once

/*
  For now, each RP consists of exactly one color output, one depth output, and 0 or more readonly input images
 * TODO polymorph ?or genericify? this for color, depth, both (use depth but don't output it), raytrace, compute etc,
 */
class RenderPass : public WITE::RenderPass {
public:
  /*Pass an image source to each camera on creation
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
  void requestResize(WITE::IntBox3D screensize);//override from interface
  BackedImage* getImages();//override from interface
  constexpr size_t getImageCount() { return 2; };//override from interface
  constexpr int64_t getFormat() { return FORMAT_STD_COLOR; };//override from interface
  inline bool appliesOnLayer(WITE::renderLayerIdx i) { return layerMask & (static_cast<WITE::renderLayerIdx>(1) << i); };
  inline void setLayermask(WITE::renderLayerMask newMask) { layerMask = newMask; };
  inline void setClearColor(float r, float g, float b, float a);
  inline void setClearDepth(float depth, int32_t stencil);
private:
  void recreateFB();
  Queue* queue;
  WITE::ImageSource* inputs;
  size_t inputCount;
  uint64_t fbResourceSerial;
  WITE::IntBox3D outputSize;
  WITE::renderLayerMask layerMask;
  std::unique_ptr<VkAttachmentDescription[]> attachments;
  static constexpr VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  std::unique_ptr<VkAttachmentReference[]> inputReferences;
  VkClearValue clearColors[2] = {{{{0.2f, 0.2f, 0.2f, 0.2f}}}, {{1.0, 0}}};
  VkClearAttachment clearAttachments[2] = { {VK_IMAGE_ASPECT_COLOR_BIT, 0, {clearColors[0]}}, {VK_IMAGE_ASPECT_DEPTH_BIT, 1, clearColors[1]} };
  VkSubpassDescription subpass;
  VkRenderPassCreateInfo rpInfo;
  VkRenderPass rp;
  VkRenderPassBeginInfo beginInfo;
  VkFramebufferCreateInfo fbInfo;
  VkImageView attachmentViews[];//array, for handing to fb creation. each is disposed by the BackedImage container
  typedef struct {
    VkFramebuffer fb;
    std::unique_ptr<BackedImage> image;
  } bufferResource_t;
  bufferResource_t bufferResources[getImageCount()];
  std::unique_ptr<BackedImage> depthImage;//not out so not multi-buffered
};
