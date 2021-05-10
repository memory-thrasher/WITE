#pragma once

namespace WITE {

  class export_def Camera {
  public:
    typedefCB(Render_cb_t, void, Queue::ExecutionPlan*);
    Camera(const Camera&) = delete;
    static Camera* make(Window*, IntBox3D, Render_cb_t);//window owns camera object
    virtual ~Camera() = default;
    virtual void resize(IntBox3D) = 0;
    virtual float getPixelTangent() = 0;
    virtual float approxScreenArea(BBox3D*) = 0;
    virtual glm::dvec3 getLocation() = 0;
    virtual void setLocation(glm::dvec3) = 0;
    virtual void setMatrix(glm::dmat4&) = 0;
    virtual void setMatrix(glm::dmat4*) = 0;
    virtual Window* getWindow() = 0;
    virtual IntBox3D getScreenRect() = 0;
    virtual void setFov(double) = 0;
    virtual double getFov() = 0;
    virtual bool appliesOnLayer(renderLayerIdx i) = 0;
    virtual void setLayermaks(WITE::renderLayerMask newMask) = 0;
  protected:
    Camera() = default;
  };

}
