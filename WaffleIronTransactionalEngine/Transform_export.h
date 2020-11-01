#pragma once

namespace WITE {
  
  class Transform {
  public:
    static inline constexpr class Transform ident() noexcept {
      return Transform();
    };
    export_dec constexpr Transform();
    export_dec constexpr Transform(const glm::dmat4&);
    export_dec constexpr Transform(const glm::dmat4*);
    export_dec constexpr Transform(const Transform& other);
    export_dec /*TODO constexpr*/ Transform(const glm::dvec3 pos, const glm::dvec3 rot = glm::dvec3(), const glm::dvec3 scale = glm::dvec3(1));
    template<class Point, class member = typename Point::value_type>
    constexpr void batchTransformPoints(Point* inout, size_t len) const {
      for(size_t i = 0;i < len;i++)
	inout[i] = Point(glm::vec<4, member, glm::defaultp>(inout[i], 1) * matrix);
    }
    export_dec constexpr BBox3D* transform(const BBox3D* in, BBox3D* out = NULL) const;
    export_dec constexpr glm::mat4 project(const glm::mat4& other) const;
    export_dec constexpr glm::mat4 project(const glm::mat4* other) const;
    export_dec constexpr glm::dmat4 project(const glm::dmat4& other) const;
    export_dec constexpr glm::dmat4 project(const glm::dmat4* other) const;
    export_dec constexpr glm::dmat4 getMat() const;
    export_dec glm::dmat4 getInvMat() const;
    export_dec constexpr glm::dvec3 getLocation() const;
    export_dec constexpr glm::dvec3 getEulerAngle() const;//expensive, try to avoid. //TODO test this
    export_dec constexpr glm::dvec3 forward() const;
    export_dec constexpr glm::dvec3 right() const;
    export_dec constexpr glm::dvec3 up() const;
    export_dec inline Transform getInv() const;
    export_dec Transform& setLocation(glm::dvec3 nl);
    export_dec Transform& setEulerAngle(glm::dvec3 nl);
    export_dec Transform& setMat(glm::dmat4* in);
    export_dec Transform& operator=(glm::dmat4&& o);
    export_dec static /*TODO constexpr*/ glm::dmat4 getEulerAngleMatrix(glm::dvec3 nl);
  private:
    glm::dmat4 matrix;
  };

};

