#pragma once

//TODO split camera, which represents a 3d coord capable of rendering, from viewport, a section of a window that gets an image
/*
TODO cube mode? Impacts: image flags (cube), image layers (6), view type (cube), sampling (x,y,z direction), phys device feature test (imageCubeArray)
cube = 6 layers in order +x, -x, +y, -y, +z, -z
also see: phys device limit maxImageDimensionCube
*/
class export_def WITE::Camera {
 public:
  Camera(const Camera&) = delete;
  virtual ~Camera() = default;
  virtual void resize(IntBox3D) = 0;
  virtual float getPixelTangent() = 0;
  virtual float approxScreenArea(BBox3D*) = 0;
  virtual glm::dvec3 getLocation() = 0;
  virtual void setLocation(glm::dvec3) = 0;
  virtual void setMatrix(glm::dmat4&) = 0;
  virtual void setMatrix(glm::dmat4*) = 0;
  virtual IntBox3D getScreenRect() = 0;
  virtual void setFov(double) = 0;
  virtual double getFov() = 0;
  virtual void render() = 0;
 protected:
  Camera() = default;
};
