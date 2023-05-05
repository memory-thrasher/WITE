#pragma once

namespace WITE::Collections {

  template<class K, class V> struct StructuralPair {
    K k;
    V v;
    constexpr StructuralPair() {};
    constexpr StructuralPair(K k, V v) : k(k), v(v) {};
    constexpr StructuralPair(K k) : k(k), v() {};//implicit constructor from k for searching by key via operator<
    constexpr inline bool operator<(const StructuralPair<K, V>& o) const { return k < o.k; };
    // constexpr inline bool operator<(const K& o) const { return k < o; };
    constexpr ~StructuralPair() = default;
  };

}
