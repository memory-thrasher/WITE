#pragma once

#include <initializer_list>
#include <iterator>

#include "StructuralPair.hpp"
#include "StdExtensions.hpp"
#include "LiteralList.hpp" //for countIL

namespace WITE::Collections {

  template<class K, class V> class LiteralMapBase {
  public:
    constexpr LiteralMapBase() = default;
    constexpr ~LiteralMapBase() = default;
  };

  template<class K, class V, size_t count> class LiteralMap : public LiteralMapBase<K, V> {
  public:
    typedef StructuralPair<K, V> P;
    Collections::CopyableArray<P, count> data;
    consteval LiteralMap(const std::initializer_list<P> il) {
      std::vector<P> sorted(il.begin(), il.end());
      std::sort(sorted.begin(), sorted.end());
      for(size_t i = 0;i < sorted.size();i++)
	data[i] = sorted[i];
    };
    // constexpr LiteralMap(const Collections::CopyableArray<P, count> il) : data(il) {
    //   std::sort(data.begin(), data.end());
    // };
    constexpr ~LiteralMap() = default;
    constexpr const V& operator[](const K& i) const { return std::lower_bound(&data[0], &data[count], P(i))->v; };
    constexpr bool contains(const K& i) const { return std::binary_search(&data[0], &data[count], P(i)); };
  };

#define defineLiteralMap(K, V, NOM, ...) constexpr ::WITE::Collections::LiteralMap<K, V, ::WITE::Collections::countIL<::WITE::Collections::StructuralPair<K, V>>({ __VA_ARGS__ })> NOM = { __VA_ARGS__ };

  template<class K, class V> struct PackedLiteralMap {
    const LiteralMapBase<K, V>* data;
    size_t cnt;
    constexpr PackedLiteralMap() = default;
    constexpr ~PackedLiteralMap() = default;
    template<size_t C> constexpr PackedLiteralMap(const LiteralMap<K, V, C>& o) : data(&o), cnt(C) {};
  };

  template<class K, class V, PackedLiteralMap<K, V> P> constexpr auto unpack() {
    return *(const LiteralMap<K, V, P.cnt>*)P.data;
  };

};
