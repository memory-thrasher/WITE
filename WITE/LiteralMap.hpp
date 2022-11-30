#pragma once

#include <initializer_list>
#include <iterator>

#include "StructuralPair.hpp"
#include "StdExtensions.hpp"
#include "LiteralLookup.hpp" //for countIL

namespace WITE::Collections {

  template<class K, class V, size_t count> class LiteralMap {
  public:
    typedef StructuralPair<K, V> P;
    std::array<P, count> data;
    constexpr LiteralMap(const std::initializer_list<P> il) {
      std::vector<P> sorted(il.begin(), il.end());
      std::sort(sorted.begin(), sorted.end());
      for(size_t i = 0;i < sorted.size();i++)
	data[i] = sorted[i];
    };
    constexpr const V& operator[](const K& i) const { return std::lower_bound(&data[0], &data[count], P(i))->v; };
    constexpr bool contains(const K& i) const { return std::binary_search(&data[0], &data[count], P(i)); };
  };

#define defineLiteralMap(K, V, NOM, ...) constexpr ::WITE::Collections::LiteralMap<K, V, ::WITE::Collections::countIL({ __VA_ARGS__ })> NOM = { __VA_ARGS__ };

};
