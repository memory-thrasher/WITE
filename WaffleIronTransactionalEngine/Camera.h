#pragma once

class Camera : public WITE::Camera {
public:
  Camera(WITE::IntBox3D size, Queue* graphics, Render_cb_t);
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
  inline void setLayermask(WITE::renderLayerMask in) { layerMask = in; };
  GPU* getGPU() { return graphicsQ->gpu; };
  void render(Queue::ExecutionPlan* ep);
  void blitTo(VkCommandBuffer cmd, VkImage dst);
private:
  void recreateResources();//not thread safe, call only from main thread
  WITE::renderLayerMask layerMask;
  float targetDelay = 1/60.0f;
  double fov;
  float pixelTangent;
  Queue* graphicsQ;
  Render_cb_t render_cb;
  WITE::IntBox3D screenbox;
  WITE::Window* window;
  WITE::Transform renderTransform, worldTransform;
  void updateMaths();
  RenderPass* passes;
  size_t passCount = 1;//TODO setter?
};

