#include "Internal.h"

namespace WITE {

  Transform::Transform(const glm::dvec3 pos, const glm::dvec3 rot, const glm::dvec3 scale) : matrix(getEulerAngleMatrix(rot)) {
    for(size_t i = 0;i < 3;i++) {
      matrix[i] *= scale[i];
      matrix[3][i] = pos[i];
    }
  }

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

  Transform Transform::getInv() const {
    return Transform(getInvMat());
  };

  constexpr glm::dvec3 Transform::forward() const {
    return glm::normalize(matrix[1]);
  }

  constexpr glm::dvec3 Transform::right() const {
    return glm::normalize(matrix[0]);
  }

  constexpr glm::dvec3 Transform::up() const {
    return glm::normalize(matrix[2]);
  }

  constexpr glm::dvec3 Transform::getEulerAngle() const {
    glm::dvec3 lu, lr, lf, gu, gr, gf, temp;
    lu = up();
    lr = right();
    lf = forward();
    gu = ident().up();
    gr = ident().right();
    gf = ident().forward();
    return glm::dvec3(
      asin(glm::dot(lf, gu)),//inclination
      acos(glm::dot(glm::normalize(glm::cross(lf, gu)), lr)),//roll
      acos(glm::dot(temp = glm::normalize(lf - gu * glm::dot(lf, gu)), gf)) * (temp.x < 0 ? -1 : 1) //azimuth
    );
  }

  glm::dmat4 Transform::getEulerAngleMatrix(glm::dvec3 nl) {
    glm::dmat4 n = rotate(
      rotate(
      rotate(glm::identity<glm::dmat4>(), nl.z, ident().up()),
      nl.x, ident().right()),
      nl.y, ident().forward());
    return n;
  }

  Transform& Transform::setEulerAngle(glm::dvec3 nl) {
    glm::dmat4 n = Transform::getEulerAngleMatrix(nl);
    for(size_t i = 0;i < 3;i++)
      matrix[i].xyz() = n[i].xyz();
    return *this;
  }

};
