#pragma once

class Camera : public WITE::Camera {
public:
  Camera(WITE::IntBox3D size, std::shared_ptr<WITE::ImageSource> source, Render_cb_t = NULL);
  ~Camera();
  void resize(WITE::IntBox3D);
  float getPixelTangent() { return pixelTangent; };
  float approxScreenArea(WITE::BBox3D*);
  glm::dvec3 getLocation();
  glm::dmat4 getRenderMatrix();
  void setLocation(glm::dvec3);
  void setMatrix(glm::dmat4&);
  void setMatrix(glm::dmat4*);
  WITE::IntBox3D getScreenRect() { return screenbox; };
  inline void setFov(double val) { fov = val; updateMaths(); };
  inline double getFov() { return fov; };
  void blitTo(VkCommandBuffer cmd, BackedImage* dst);
private:
  double fov;
  float pixelTangent;
  Render_cb_t render_cb;
  WITE::IntBox3D screenbox;
  WITE::Transform renderTransform, worldTransform;
  void updateMaths();
  std::shared_ptr<WITE::ImageSource> source;
};

