#include "stdafx.h"
#include "Transform.h"

namespace WITE {

  Transform::Transform(glm::dmat4 mat) : matrix(mat), hasInverse(false) {
  }

  Transform::~Transform() {}

  BBox3D* transform(const BBox3D* in, BBox3D* out = NULL) {
    //TODO
  }
  
  glm::dmat4 Transform::project(const glm::dmat4& other) {
    return other * matrix;
  }

  glm::dmat4 Transform::project(const glm::dmat4* other) {
    return *other * matrix;
  }

  glm::dmat4 Transform::getMat() {
    return glm::dmat4(matrix);//copy
  }
  
  glm::dmat4 Transform::getInvMat() {
    return getInv()->getMat();
  }
  
  glm::vec3 Transform::getLocation() {
    return glm::xyz(matrix[3]);
  }
  
  void Transform::setLocation(glm::vec3 nl) {
    matrix[3] = dvec4(nl, 1);
    inverseValid = false;
  }
  
  Transform* Transform::getInv() {
    if(!inverse)
      inverse = std::make_shared<Transform>();
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

  void setFromInverse(Transform* other) {
    matrix = glm::inverse(matrix);
    inverseValid = true;
  }

};
