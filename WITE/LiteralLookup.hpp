#pragma once

#include <initializer_list>
#include <iterator>

#include "StructuralPair.hpp"
#include "StdExtensions.hpp"
#include "LiteralList.hpp" //for countIL

namespace WITE::Collections {

  template<class K, class V>
  consteval size_t countIlOfPairsOfIl(const std::initializer_list<StructuralPair<K, std::initializer_list<V>>> il) {
    size_t ret = 0;
    for(const auto pair : il)
      ret += countIL(pair.v);
    return ret;
  };

  //one-to-many, immutable constexpr, map-like efficiency for runtime reads
  template<class K, class V, size_t keyCount, size_t valueCount> class LiteralLookup {
  public:
    struct range {
      const V* first, *last;
      constexpr range(const V* f, const V* l) : first(f), last(l) {};
      constexpr const V* begin() const { return first; };
      constexpr const V* end() const { return last; };
      constexpr inline bool contains(const V& v) const { return Collections::contains(*this, v); };
    };
    std::array<V, valueCount> data;
    typedef StructuralPair<K, size_t> index_t;
    std::array<index_t, keyCount> index;
    constexpr LiteralLookup(const std::initializer_list<StructuralPair<K, std::initializer_list<V>>> il) {
      std::vector<StructuralPair<K, std::initializer_list<V>>> sorted(il.begin(), il.end());
      std::sort(sorted.begin(), sorted.end());
      size_t insert = 0;
      for(size_t i = 0;i < sorted.size();i++) {
	index[i] = index_t(sorted[i].k, insert);
	for(auto v : sorted[i].v)
	  data[insert++] = v;
      }
    };
    constexpr range operator[](const K& i) const {
      const index_t* ii = std::lower_bound(index.begin(), index.end(), i);
      return range(&data[ii->v], &data[ii == &index[keyCount - 1] ? valueCount : ii[1].v]);
    };
    constexpr inline range at(const K& i) const { return operator[](i); };
    constexpr bool contains(const K& i) const {
      return std::binary_search(index.begin(), index.end(), index_t(i));
    };
  };

#define defineLiteralLookup(K, V, NOM, ...) constexpr ::WITE::Collections::LiteralLookup<K, V, ::WITE::Collections::countIL({ __VA_ARGS__ }), ::WITE::Collections::countIlOfPairsOfIl({ __VA_ARGS__ })> NOM = { __VA_ARGS__ };

};

