#pragma once

#include <glm/glm.hpp>

#include "StdExtensions.hpp"

namespace WITE::Util {
  
  class BBox3D {
  public:
  typedef glm::dvec3 v3;
  union {
    double component[1];
    struct { double minx, miny, minz, maxx, maxy, maxz; };
  };
  constexpr BBox3D(double minx = 0, double maxx = 0, double miny = 0, double maxy = 0, double minz = 0, double maxz = 0) :
    minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz) {};
  constexpr BBox3D(v3 min, v3 max) : BBox3D(min.x, max.x, min.y, max.y, min.z, max.z) {}
  constexpr BBox3D(glm::vec3 min, glm::vec3 max) : BBox3D(v3(min), v3(max)) {}//promote from float
  constexpr inline glm::dvec3 min() const { return glm::dvec3(minx, miny, minz); };
  constexpr inline glm::dvec3 max() const { return glm::dvec3(maxx, maxy, maxz); };
  constexpr inline glm::dvec3 center() const { return (min() + max())*0.5; };
  constexpr inline void allCornerSwizzle(glm::dvec3* out) const {
    size_t i = 0;
    for(size_t x = 0;x < 2;x++) for(size_t y = 0;y < 2;y++) for(size_t z = 0;z < 2;z++, i++)
      out[i] = glm::dvec3(component[x*3], component[y*3+1], component[z*3+2]);
  };
  constexpr inline double width2D() const { return maxx - minx; };
  constexpr inline double height2D() const { return maxy - miny; };
  };

  class IntBox3D {
  public:
  union {
    uint64_t comp[1];
    struct { uint64_t minx, miny, minz, maxx, maxy, maxz; };
  };
  IntBox3D(uint64_t minx = 0, uint64_t maxx = 0, uint64_t miny = 0, uint64_t maxy = 0, uint64_t minz = 0, uint64_t maxz = 0) :
  minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz) {};
  inline int width()  { return (int)(maxx - minx); };
  inline int height() { return (int)(maxy - miny); };
  inline int depth()  { return (int)(maxz - minz); };
  constexpr inline glm::ivec3 min() const { return glm::ivec3(minx, miny, minz); };
  constexpr inline glm::ivec3 max() const { return glm::ivec3(maxx, maxy, maxz); };
  constexpr inline glm::ivec3 center() const { return (min() + max())/2; };
  inline bool operator==(IntBox3D& o) { return memcmp(comp, o.comp, sizeof(comp)) == 0; };
  inline bool sameSize(IntBox3D& o) { return o.maxx - o.minx == maxx - minx && o.maxy - o.miny == maxy - miny && o.maxz - o.minz == maxz - minz; };
  };

  template<class T, class Compare = std::less<T>> inline T clamp(T t, T min, T max) {
    Compare lt();
    return lt(t, max) ? lt(min, t) ? t : min : max;
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
  public:
    inline T operator()(const T* a, const T* b) const {
      T ret;
      for(size_t i = 0;i < components;i++)
	ret[i] = max((*a)[i], (*b)[i]);
      return ret;
    };
  };

  template<class V> using Mangle_MaxVec = Mangle_ComponentwiseMax<V::length(), typename V::type>;
  typedef Mangle_MaxVec<glm::vec4> Mangle_MaxVec4;
  typedef Mangle_MaxVec<glm::vec3> Mangle_MaxVec3;
  typedef Mangle_MaxVec<glm::vec2> Mangle_MaxVec2;

  template<size_t components, typename T> class Mangle_ComponentwiseMin {
  public:
    inline T operator()(const T* a, const T* b) const {
      T ret;
      for(size_t i = 0;i < components;i++)
	ret[i] = min((*a)[i], (*b)[i]);
      return ret;
    };
  };

  template<class V> using Mangle_MinVec = Mangle_ComponentwiseMin<V::length(), typename V::type>;
  typedef Mangle_MinVec<glm::vec4> Mangle_MinVec4;
  typedef Mangle_MinVec<glm::vec3> Mangle_MinVec3;
  typedef Mangle_MinVec<glm::vec2> Mangle_MinVec2;

  template<class T> class Mangle_Sum {
  public:
    inline T operator()(const T* a, const T* b) const {
      return *a + *b;
    };
  };

  template<class Mangler, class T, class U> inline T mangle(const T* a, const U* b) {
    return Mangler()(a, b);
  };

  template<class Mangler, class T, class U, class V, class... WX> T mangle(const T* a, const U* b, const V* c, const WX... more) {
    U bx = mangle<Mangler>(b, c, std::forward<WX>(more)...);
    return mangle<Mangler>(a, bx);
  };

  template<class Mangler, class T> void mangle(const T* in, size_t count, T* ret) {
    if(!count) return;
    *ret = in[0];//copy constructable
    Mangler m;
    for(size_t i = 1;i < count;i++)
      *ret = m(ret, &in[i]);
  };

  template<class Mangler, class T> T mangle(const T* in, size_t count) {
    T ret;
    mangle<Mangler, T>(in, count, &ret);
    return ret;
  };

  template<class R> R floor(double d) {
    return static_cast<R>(d);
  };

  //opposite of roll operator. In a = 1 << c, find c given a. Index of the lowest bit that is set in the input.
  constexpr size_t unroll(uint64_t v) {
    if(!v) return ~0;
    for(size_t i = 0;i < 64;i++)
      if(v & (1 << i))
	return i;
    return ~1;//impossible
  };

  //opposite of roll operator. In a = 1 << c, find c given a. Index of the Nth lowest bit that is set in the input.
  constexpr size_t unrollNth(uint64_t v, size_t n) {
    if(!v) return ~0;
    for(size_t i = 0;i < 64;i++)
      if(v & (1 << i))
	if(n-- == 0)
	  return i;
    return ~1;
  };

};
