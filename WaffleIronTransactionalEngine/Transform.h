#pragma once

namespace WITE {
  class BBox3D;

  class Transform {
  public:
    Transform(glm::dmat4 = glm::identity<glm::dmat4>());
    ~Transform();
    const BBox3D* transform(const BBox3D* in, BBox3D* out = NULL);
    const glm::mat4 project(const glm::mat4& other);
    const glm::mat4 project(const glm::mat4* other);
    const glm::dmat4 project(const glm::dmat4& other);
    const glm::dmat4 project(const glm::dmat4* other);
    const glm::dmat4 getMat();
    const glm::dmat4 getInvMat();
    const glm::vec3 getLocation();
    void setLocation(glm::vec3 nl);
    const Transform* getInv();
    void setMat(glm::dmat4* in);
    Transform& operator=(glm::dmat4&& o);
  private:
    void setFromInverse(Transform* other);
    glm::dmat4 matrix;
    mutable std::unique_ptr<Transform> inverse;
    mutable bool inverseValid = false;
  };

};

