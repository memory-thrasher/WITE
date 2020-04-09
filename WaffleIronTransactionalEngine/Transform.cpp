#include "stdafx.h"
#include "Transform.h"

namespace WITE {

  Transform::Transform(glm::dmat4 mat) : matrix(mat), inverseValid(false) {
  }

  Transform::~Transform() {}

  BBox3D* Transform::transform(const BBox3D* in, BBox3D* out) const {
    static BBox3D spareRet;
    glm::dvec3 corners[8];
    in->allCornerSwizzle(corners);//read all inputs before writing to out, so in = out ( = spareRet) is allowed
    batchTransformPoints(corners, 8);
    if(!out) out = &spareRet;
    *out = BBox3D(mangle<Mangle_MinVec<glm::dvec4>>(corners, 8), mangle<Mangle_MaxVec<glm::dvec4>>(corners, 8));
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
    return getInv()->getMat();
  }
  
  glm::vec3 Transform::getLocation() const {
    return glm::dvec3(matrix[3]);
  }
  
  void Transform::setLocation(glm::vec3 nl) {
    matrix[3] = glm::dvec4(nl, 1);
    inverseValid = false;
  }
  
  const Transform* Transform::getInv() const {
    if(!inverse)
      inverse = std::make_unique<Transform>();
    if(!inverseValid) {
      inverseValid = true;
      inverse->setFromInverse(this);
    }
    return inverse.get();
  }
  
  void Transform::setMat(glm::dmat4* in) {
    matrix = *in;
    inverseValid = false;
  }

  Transform& Transform::operator=(glm::dmat4&& o) {
    setMat(&o);
    return *this;
  };

  void Transform::setFromInverse(const Transform* other) {
    matrix = glm::inverse(matrix);
    inverseValid = true;
  }

};
