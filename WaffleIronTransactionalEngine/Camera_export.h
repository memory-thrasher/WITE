#pragma once

namespace WITE {

  //TODO split camera, which represents a 3d coord capable of rendering, from viewport, a section of a window that gets an image
  class export_def Camera {
  public:
    Camera(const Camera&) = delete;
    static Camera* make(Window*, IntBox3D);//window owns camera object
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
  protected:
    Camera() = default;
  };

}
