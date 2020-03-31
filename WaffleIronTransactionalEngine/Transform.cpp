#include "stdafx.h"
#include "Transform.h"


Transform::Transform(glm::dmat4 mat) : matrix(mat), hasInverse(false) {
}


Transform::~Transform()
{
}

Transform& Transform::operator=(glm::dmat4&& o) {
  setMat(&o);
  return *this;
};
