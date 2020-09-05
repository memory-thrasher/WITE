#include "stdafx.h"
#include "Transform.h"

namespace WITE {

  constexpr Transform::Transform(const glm::dmat4& mat) : matrix(mat) {}

  constexpr Transform::Transform(const glm::dmat4* mat) : matrix(*mat) {}

  constexpr Transform::Transform() : matrix(glm::identity<glm::dmat4>()) {}

  constexpr Transform::Transform(const Transform& other) : Transform(other.matrix) {}

  constexpr BBox3D* Transform::transform(const BBox3D* in, BBox3D* out) const {
    static BBox3D spareRet;//TODO thread safety
    glm::dvec3 corners[8];
    in->allCornerSwizzle(corners);//read all inputs before writing to out, so in = out ( = spareRet) is allowed
    batchTransformPoints(corners, 8);
    if(!out) out = &spareRet;
    *out = BBox3D(mangle<Mangle_MinVec<glm::dvec3>>(corners, 8), mangle<Mangle_MaxVec<glm::dvec3>, glm::dvec3>(corners, 8));
    return out;
  }
  
  constexpr glm::dmat4 Transform::project(const glm::dmat4& other) const {
    glm::dmat4 ret;
    for(size_t col = 0; col < 4;col++)
      for(size_t row = 0;row < 4;row++) {
        ret[col][row] = 0;
        for(size_t i = 0;i < 4;i++)
          ret[col][row] += other[i][row] * matrix[col][i];
      }
    return ret;
  }

  constexpr glm::dmat4 Transform::project(const glm::dmat4* other) const {
    glm::dmat4 ret;
    for(size_t col = 0; col < 4;col++)
      for(size_t row = 0;row < 4;row++) {
        ret[col][row] = 0;
        for(size_t i = 0;i < 4;i++)
          ret[col][row] += (*other)[i][row] * matrix[col][i];
      }
    return ret;
  }

  constexpr glm::dmat4 Transform::getMat() const {
    return glm::dmat4(matrix);//copy
  }
  
  glm::dmat4 Transform::getInvMat() const {//TODO constexpr
    return glm::inverse(matrix);
  }
  
  constexpr glm::dvec3 Transform::getLocation() const {
    return glm::dvec3(matrix[3]);
  }
  
  Transform& Transform::setLocation(glm::dvec3 nl) {
    matrix[3] = glm::dvec4(nl, 1);
    return *this;
  }
  
  Transform& Transform::setMat(glm::dmat4* in) {
    matrix = *in;
    return *this;
  }

  Transform& Transform::operator=(glm::dmat4&& o) {
    setMat(&o);
    return *this;
  };

  Transform& Transform::getInv() const {
    return Transform(getInvMat());
  };

  constexpr glm::dvec3 Transform::forward() const {
    return matrix[1];
  }

  constexpr glm::dvec3 Transform::right() const {
    return matrix[0];
  }

  constexpr glm::dvec3 Transform::up() const {
    return matrix[2];
  }

  constexpr glm::dvec3 Transform::getAxisAngle() const {
    glm::dvec3 u, r, f;
    u = up();
    r = right();
    f = forward();
    return glm::dvec3(0, 0, 0);//TODO
  }

  Transform& Transform::setAxisAngle(glm::dvec3 nl) {
    glm::dmat4 n = rotate(
      rotate(
        rotate(glm::identity<glm::dmat4>(), nl.y, ident().forward()),
      nl.x, ident().right()),
    nl.z, ident().up());
    for(size_t i = 0;i < 3;i++)
      matrix[i].xyz = n[i].xyz;
    return *this;
  }

};
