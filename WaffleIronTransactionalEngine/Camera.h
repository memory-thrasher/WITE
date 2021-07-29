#pragma once

class Camera : public WITE::Camera {
public:
  Camera(WITE::IntBox3D size, Queue* graphics, Render_cb_t);
  ~Camera();
  void resize(WITE::IntBox3D);
  float getPixelTangent() { return pixelTangent; };
  float approxScreenArea(WITE::BBox3D*);
  glm::dvec3 getLocation();
  glm::dmat4 getRenderMatrix();
  void setLocation(glm::dvec3);
  void setMatrix(glm::dmat4&);
  void setMatrix(glm::dmat4*);
  WITE::Window* getWindow() { return window; };
  WITE::IntBox3D getScreenRect() { return screenbox; };
  inline void setFov(double val) { fov = val; updateMaths(); };
  inline double getFov() { return fov; };
  GPU* getGPU() { return graphicsQ->gpu; };
  void blitTo(VkCommandBuffer cmd, BackedImage* dst);
private:
  float targetDelay = 1/60.0f;
  double fov;
  float pixelTangent;
  Queue* graphicsQ;
  Render_cb_t render_cb;
  WITE::IntBox3D screenbox;
  WITE::Window* window;
  WITE::Transform renderTransform, worldTransform;
  void updateMaths();
  //TODO hand in RPs on creation
  RenderPass* passes;
  size_t passCount = 1;//TODO setter?
};

