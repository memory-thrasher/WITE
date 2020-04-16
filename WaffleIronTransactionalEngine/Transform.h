#pragma once

#include "WMath.h"

namespace WITE {
  
  class BBox3D;

  class Transform {
  public:
    Transform(glm::dmat4 = glm::identity<glm::dmat4>());
    Transform(const Transform& other);
    ~Transform();
    template<class Point, class member = typename Point::value_type>
    void batchTransformPoints(Point* inout, size_t len) const {
      for(size_t i = 0;i < len;i++)
	inout[i] = Point(glm::vec<4, member, glm::defaultp>(inout[i], 1) * matrix);
    }
    BBox3D* transform(const BBox3D* in, BBox3D* out = NULL) const;
    glm::mat4 project(const glm::mat4& other) const;
    glm::mat4 project(const glm::mat4* other) const;
    glm::dmat4 project(const glm::dmat4& other) const;
    glm::dmat4 project(const glm::dmat4* other) const;
    glm::dmat4 getMat() const;
    glm::dmat4 getInvMat() const;
    glm::vec3 getLocation() const;
    Transform getInv() const;
    void setLocation(glm::vec3 nl);
    void setMat(glm::dmat4* in);
    Transform& operator=(glm::dmat4&& o);
  private:
    glm::dmat4 matrix;
  };

};

