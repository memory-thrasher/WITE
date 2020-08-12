#include "stdafx.h"
#include "Transform.h"

namespace WITE {

  Transform::Transform(const glm::dmat4& mat) : matrix(mat) {}

  Transform::Transform(const glm::dmat4* mat) : matrix(*mat) {}

  Transform::Transform() : matrix(glm::identity<glm::dmat4>()) {}

  Transform::Transform(const Transform& other) : Transform(other.matrix) {}

  Transform::~Transform() {}

  BBox3D* Transform::transform(const BBox3D* in, BBox3D* out) const {
    static BBox3D spareRet;//TODO thread safety
    glm::dvec3 corners[8];
    in->allCornerSwizzle(corners);//read all inputs before writing to out, so in = out ( = spareRet) is allowed
    batchTransformPoints(corners, 8);
    if(!out) out = &spareRet;
    *out = BBox3D(mangle<Mangle_MinVec<glm::dvec3>>(corners, 8), mangle<Mangle_MaxVec<glm::dvec3>, glm::dvec3>(corners, 8));
    return out;
  }
  
  glm::dmat4 Transform::project(const glm::dmat4& other) const {
    return other * matrix;
  }

  glm::dmat4 Transform::project(const glm::dmat4* other) const {
    return *other * matrix;
  }

  glm::dmat4 Transform::getMat() const {
    return glm::dmat4(matrix);//copy
  }
  
  glm::dmat4 Transform::getInvMat() const {
    return glm::inverse(matrix);
  }
  
  glm::vec3 Transform::getLocation() const {
    return glm::dvec3(matrix[3]);
  }
  
  void Transform::setLocation(glm::vec3 nl) {
    matrix[3] = glm::dvec4(nl, 1);
  }
  
  void Transform::setMat(glm::dmat4* in) {
    matrix = *in;
  }

  Transform& Transform::operator=(glm::dmat4&& o) {
    setMat(&o);
    return *this;
  };

  Transform Transform::getInv() const {
    return Transform(getInvMat());
  };

};
