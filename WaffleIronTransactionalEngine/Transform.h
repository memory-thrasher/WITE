#pragma once

#include "WMath.h"

namespace WITE {
  
  class BBox3D;

  class Transform {
  public:
    Transform(glm::dmat4 = glm::identity<glm::dmat4>());
    ~Transform();
    template<size_t components, class member>
    void batchTransformPoints(glm::vec<components, member, glm::defaultp>* inout, size_t len) const {
      for(size_t i = 0;i < len;i++)
	inout[i] = glm::dvec3(glm::dvec4(inout[i], 1) * matrix);
    }
    BBox3D* transform(const BBox3D* in, BBox3D* out = NULL) const;
    glm::mat4 project(const glm::mat4& other) const;
    glm::mat4 project(const glm::mat4* other) const;
    glm::dmat4 project(const glm::dmat4& other) const;
    glm::dmat4 project(const glm::dmat4* other) const;
    glm::dmat4 getMat() const;
    glm::dmat4 getInvMat() const;
    glm::vec3 getLocation() const;
    void setLocation(glm::vec3 nl);
    const Transform* getInv() const;
    void setMat(glm::dmat4* in);
    Transform& operator=(glm::dmat4&& o);
  private:
    void setFromInverse(const Transform* other);
    glm::dmat4 matrix;
    mutable std::unique_ptr<Transform> inverse;
    mutable bool inverseValid = false;
  };

};

