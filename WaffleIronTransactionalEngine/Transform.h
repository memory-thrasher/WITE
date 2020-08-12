#pragma once

#include "WMath.h"

namespace WITE {
  
  class BBox3D;

  export_dec class Transform {
  public:
    export_dec Transform();
    export_dec Transform(const glm::dmat4&);
    export_dec Transform(const glm::dmat4*);
    export_dec Transform(const Transform& other);
    export_dec ~Transform();
    template<class Point, class member = typename Point::value_type>
    void batchTransformPoints(Point* inout, size_t len) const {
      for(size_t i = 0;i < len;i++)
	inout[i] = Point(glm::vec<4, member, glm::defaultp>(inout[i], 1) * matrix);
    }
    export_dec BBox3D* transform(const BBox3D* in, BBox3D* out = NULL) const;
    export_dec glm::mat4 project(const glm::mat4& other) const;
    export_dec glm::mat4 project(const glm::mat4* other) const;
    export_dec glm::dmat4 project(const glm::dmat4& other) const;
    export_dec glm::dmat4 project(const glm::dmat4* other) const;
    export_dec glm::dmat4 getMat() const;
    export_dec glm::dmat4 getInvMat() const;
    export_dec glm::vec3 getLocation() const;
    export_dec Transform getInv() const;
    export_dec void setLocation(glm::vec3 nl);
    export_dec void setMat(glm::dmat4* in);
    export_dec Transform& operator=(glm::dmat4&& o);
  private:
    glm::dmat4 matrix;
  };

};

