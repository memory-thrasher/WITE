#pragma once


class Camera : public WITE::Camera {
public:
  Camera(WITE::IntBox3D size, Queue* graphics, Queue* present);
  ~Camera();
  void resize(WITE::IntBox3D);
  float getPixelTangent() { return pixelTangent; };
  float approxScreenArea(WITE::BBox3D*);
  glm::dvec3 getLocation();
  void setLocation(glm::dvec3);
  void setMatrix(glm::dmat4&);
  void setMatrix(glm::dmat4*);
  WITE::Window* getWindow() { return window; };
  WITE::IntBox3D getScreenRect() { return screenbox; };
  inline void setFov(double val) { fov = val; updateMaths(); };
  inline double getFov() { return fov; };
  inline bool appliesOnLayer(WITE::renderLayerIdx i) { return layerMask & (1ull << i); };
  inline void setLayermaks(WITE::renderLayerMask in) { layerMask = in; };
  GPU* getGPU() { return presentQ->gpu; };
  void render(std::shared_ptr<Queue::ExecutionPlan> ep);
  void blitTo(VkCommandBuffer cmd, VkImage dst);
private:
  void recreateResources();//not thread safe, call only from main thread
  WITE::renderLayerMask layerMask;
  float targetDelay = 1/60.0f;
  double fov;
  float pixelTangent;
  Queue* presentQ, *graphicsQ;
  WITE::IntBox3D screenbox;
  WITE::Window* window;
  WITE::Transform renderTransform, worldTransform;
  void updateMaths();
  typedef struct RenderPass_t {
    static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D16_UNORM, COLOR_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
    static constexpr VkAttachmentDescription attachments[2] =
      {{0, COLOR_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
       {0, DEPTH_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
    static constexpr VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
      depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    static constexpr VkSubpassDescription subpass = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &colorReference, NULL, &depthReference, 0, NULL };
    static constexpr VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 2, attachments, 1, &subpass, 0, NULL};
    static constexpr VkClearValue CLEAR[2] = {{{{0.2f, 0.2f, 0.2f, 0.2f}}}, {{1.0, 0}}};
    static constexpr VkClearAttachment CLEAR_ATTACHMENTS[2] = {
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{0.5f, 0.5f, 0.5f, 0.5f}}} },
      { VK_IMAGE_ASPECT_DEPTH_BIT, 1, {{1.0, 0}} }
    };
    std::unique_ptr<BackedImage> color, depth;
    VkRenderPass rp;
    VkFramebuffer fb;//output of camera, input to blit op that writes to window's swapchain image.
    VkRenderPassBeginInfo beginInfo;
  } RenderPass;
  RenderPass* passes = NULL;
  size_t passCount = 1;//TODO setter?
};

