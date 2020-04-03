#pragma once

#include "Transform.h"
#include "Queue.h"
#include "BackedImage.h"

class Camera : WITE::Camera {
public:
  Camera(IntBox3D size, Queue* present);
  ~Camera();
  void resize(IntBox3D);
  float getPixelTangent() { return pixelTangent; };
  float approxScreenArea(BBox3D*);
  glm::vec3 getLocation();
  void setLocation(glm::vec3);
  Window* getWindow() { return window; };
  IntBox3D getScreenRect() { return screenbox; };
  inline void setFov(double val) { fov = val; updateMaths(); };
  inline double getFov() { return fov; };
  inline bool appliesOnLayer(renderLayerIdx i) { return layerMask & (1 << i); };
  GPU* getGPU() { return presentQ->gpu; };
  void render(Queue::ExecutionPlan* ep);
  void blitTo(VkCommandBuffer cmd, VkImage dst);
private:
  void recreateResources();//not thread safe, call only from main thread
  renderLayerMask layerMask;
  float targetDelay = 1/60.0f;
  double fov, pixelTangent;
  Queue* presentQ;
  IntBox3D screenbox;
  Window* window;
  Transform renderTransform, worldTransform;
  void updateMaths();
  typedef struct {
    const VkFormat DEPTH_FORMAT = VK_FORMAT_D16_UNORM, COLOR_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
    const VkAttachmentDescription attachments[2] =
      {{0, COLOR_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
       {0, DEPTH_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
    const VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
      depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    const VkSubpassDescription subpass = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &colorReference, NULL, &depthReference, 0, NULL };
    const VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 2, attachments, 1, &subpass, 0, NULL};
    const VkClearValue CLEAR[2] = {{{0.2f, 0.2f, 0.2f, 0.2f}, {1f, 0}}, {{0.2f, 0.2f, 0.2f, 0.2f}, {1f, 0}}};
    std::unique_ptr<BackedImage> color, depth;
    VkRenderPass rp;
    VkFramebuffer fb;//output of camera, input to blit op that writes to window's swapchain image.
    VkRenderPassBeginInfo beginInfo;
  } RenderPass;
  RenderPass* passes;
  size_t passCount = 1;//TODO setter?
};

