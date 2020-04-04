#pragma once

namespace WITE {
  class BBox3D;
};

class Transform
{
public:
  Transform(glm::dmat4 = glm::identity<glm::dmat4>());
  ~Transform();
  WITE::BBox3D* transform(const WITE::BBox3D* in, WITE::BBox3D* out = NULL);
  glm::mat4 project(const glm::mat4& other);
  glm::mat4 project(const glm::mat4* other);
  glm::dmat4 project(const glm::dmat4& other) { return other * matrix; };
  glm::dmat4 project(const glm::dmat4* other);
  glm::dmat4 getMat();
  glm::dmat4 getInvMat();
  glm::vec3 getLocation();
  void setLocation(glm::vec3 nl);
  Transform* getInv();
  void setMat(glm::dmat4* in);
  Transform& operator=(glm::dmat4&& o) { setMat(&o); return *this; };
private:
  glm::dmat4 matrix, inverse;
  bool hasInverse;
};

