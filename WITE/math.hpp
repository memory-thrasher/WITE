/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <glm/glm.hpp>

#include "stdExtensions.hpp"
#include "DEBUG.hpp"

namespace WITE {
  
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

  class intBox3D {
  public:
  union {
    uint64_t comp[1];
    struct { uint64_t minx, miny, minz, maxx, maxy, maxz; };
  };
  intBox3D(uint64_t minx = 0, uint64_t maxx = 0, uint64_t miny = 0, uint64_t maxy = 0, uint64_t minz = 0, uint64_t maxz = 0) :
  minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz) {};
  inline uint64_t width()  { return (int)(maxx - minx); };
  inline uint64_t height() { return (int)(maxy - miny); };
  inline uint64_t depth()  { return (int)(maxz - minz); };
  constexpr inline glm::ivec3 min() const { return glm::ivec3(minx, miny, minz); };
  constexpr inline glm::ivec3 max() const { return glm::ivec3(maxx, maxy, maxz); };
  constexpr inline glm::ivec3 center() const { return (min() + max())/2; };
  inline bool operator==(intBox3D& o) { return memcmp(comp, o.comp, sizeof(comp)) == 0; };
  inline bool sameSize(intBox3D& o) { return o.maxx - o.minx == maxx - minx && o.maxy - o.miny == maxy - miny && o.maxz - o.minz == maxz - minz; };
  };

  template<class T, class Compare = std::less<T>> inline T constexpr clamp(T t, T min, T max) {
    Compare lt;
    return lt(t, max) ? lt(min, t) ? t : min : max;
  };

  template<class T, class U, class Compare = std::less<T>> constexpr inline T min(T a, U b) {
    return Compare()(a, b) ? a : T(b);
  };

  template<class T, class U, class Compare = std::less<T>, class V, class... WX>
  constexpr inline T min(T a, U b, V c, WX... more) {
    U bx = min<U, V, Compare, WX...>(b, c, std::forward<WX>(more)...);
    return min<T, U, Compare>(a, bx);
  };
  
  template<class T, class U, class Compare = std::less<T>> constexpr inline T max(T a, U b) {
    return Compare()(a, b) ? T(b) : a;
  };

  template<class T, class U, class Compare = std::less<T>, class V, class... WX>
  constexpr inline T max(T a, U b, V c, WX... more) {
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

  //floats have precision issues with too many significant digits left of the decimal. This helps with those cases.
  inline float intModFloat(uint64_t numerator, float denominator) {
    if(denominator < 0) [[unlikely]] denominator *= -1;
    float numeratorFloatPart = 0, tempIntPart = 0;
    float ret = numerator;
    while(ret >= denominator) {
      //multiplying a float by a power of 2 does not loose precision
      int exp = (int)std::log2f((ret) / denominator);
      float tempDenom = denominator * (1 << exp);
      if(tempDenom >= ret) [[unlikely]] {//precision loss leading into log2f
	--exp;
	tempDenom = denominator * (1 << exp);
      }
      if(exp < 0) [[unlikely]] break;
      ASSERT_TRAP(tempDenom > 0, "denominator too small?");
      ASSERT_TRAP(tempDenom < ret, "modulus overrun");
      //subtracting a multiple of the denominator does not change the modulus
      numeratorFloatPart -= std::modf(tempDenom, &tempIntPart);
      numerator -= static_cast<uint64_t>(tempIntPart + 0.1f);//+0.1f: avoid rounding 0.9999f down (result "should be" an int)
      if(numeratorFloatPart < -1) {
	ASSERT_TRAP(numerator + numeratorFloatPart > 0, "numerator parts too diverged");
	numeratorFloatPart = std::modf(numeratorFloatPart, &tempIntPart);
	numerator += static_cast<uint64_t>(tempIntPart - 0.1f);
      }
      ret = numerator + numeratorFloatPart;
    }
    if(ret == denominator) [[unlikely]] ret = 0;//yes this happens
    ASSERT_TRAP(ret >= 0 && ret < denominator, "modulus fail");
    return ret;
  };

};
