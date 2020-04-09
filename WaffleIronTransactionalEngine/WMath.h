#pragma once

#include "constants.h"

namespace WITE {
  
  class export_def BBox3D {
  public:
  union {
    double component[1];
    glm::dvec3 corner[1];
    struct { double minx, miny, minz, maxx, maxy, maxz, centerx, centery, centerz; };
    struct { glm::dvec3 min, max, center; };//sizeof glm::dvec3 is not clear, may contain functions. TODO test this.
  };
  BBox3D(double minx = 0, double maxx = 0, double miny = 0, double maxy = 0, double minz = 0, double maxz = 0) :
  BBox3D(glm::dvec3(minx, miny, minz), glm::dvec3(maxx, maxy, maxz)) {};
  BBox3D(glm::dvec3 min, glm::dvec3 max) : min(min), max(max), center((min + max) * 0.5) {};
  void allCornerSwizzle(glm::dvec3* out) const {
    size_t i = 0;
    for(size_t x = 0;x < 2;x++) for(size_t y = 0;y < 2;y++) for(size_t z = 0;z < 2;z++, i++) out[i] = glm::dvec3(corner[x].x, corner[y].y, corner[z].z);
  };
  inline double width2D() { return maxx - minx; };
  inline double height2D() { return maxy - miny; };
  };

  class export_def IntBox3D {
  public:
  union {
    uint64_t comp[1];
    struct { uint64_t minx, miny, minz, maxx, maxy, maxz, centerx, centery, centerz, width, height, depth; };
  };
  IntBox3D(uint64_t minx = 0, uint64_t maxx = 0, uint64_t miny = 0, uint64_t maxy = 0, uint64_t minz = 0, uint64_t maxz = 0) :
  minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz),
  centerx((maxx+minx)/2), centery((maxy + miny) / 2), centerz((maxz + minz) / 2),
  width(maxx - minx), height(maxy - miny), depth(maxz - minz) {};
  inline bool operator==(IntBox3D& o) { return memcmp((void*)comp, (void*)o.comp, 6 * sizeof(minx)) == 0; };
  inline bool sameSize(IntBox3D& o) { return o.maxx - o.minx == maxx - minx && o.maxy - o.miny == maxy - miny && o.maxz - o.minz == maxz - minz; };
  };

  template<class T, class U, class Compare = std::less<T>> inline T min(T a, U b) {
    return Compare()(a, b) ? a : T(b);
  };

  template<class T, class U, class V, class Compare = std::less<T>, class... WX> inline T min(T a, U b, V c, WX... more) {
    U bx = min<U, V, Compare, WX...>(b, c, std::forward<WX>(more)...);
    return min<T, U, Compare>(a, bx);
  };
  
  template<class T, class U, class Compare = std::less<T>> inline T max(T a, U b) {
    return Compare()(a, b) ? T(b) : a;
  };

  template<class T, class U, class V, class Compare = std::less<T>, class... WX> inline T max(T a, U b, V c, WX... more) {
    U bx = max<U, V, Compare, WX...>(b, c, std::forward<WX>(more)...);
    return max<T, U, Compare>(a, bx);
  };

  template<size_t components, class T> class Mangle_ComponentwiseMax {
    inline T& operator()(T& a, T& b) const {
      T ret;
      for(size_t i = 0;i < components;i++)
	ret[i] = max(a[i], b[i]);
      return ret;
    };
  };

  template<class V> using Mangle_MaxVec = Mangle_ComponentwiseMax<V::length(), typename V::type>;

  template<size_t components, class T> class Mangle_ComponentwiseMin {
    inline T& operator()(T& a, T& b) const {
      T ret;
      for(size_t i = 0;i < components;i++)
	ret[i] = min(a[i], b[i]);
      return ret;
    };
  };

  template<class V> using Mangle_MinVec = Mangle_ComponentwiseMin<V::length(), typename V::type>;

  template<class Mangler, class T, class U> inline T& mangle(T& a, U& b) {
    return Mangler()(a, b);
  };

  template<class Mangler, class T, class U, class V, class... WX> T& mangle(T& a, U& b, V& c, WX... more) {
    U bx = mangle<Mangler>(b, c, std::forward<WX>(more)...);
    return mangle<Mangler>(a, bx);
  };

  template<class Mangler, class T> inline T& mangle(T* in, size_t count) {
    T ret = in[0];//copy constructable
    for(size_t i = 1;i < count;i++)
      ret = min(ret, in[i]);
    return ret;
  }

};
