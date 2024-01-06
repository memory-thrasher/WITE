#pragma once

#include <memory>
#include <initializer_list>
#include <algorithm>

#include "StdExtensions.hpp"
#include "StructuralPair.hpp"

namespace WITE::Collections {

  //map-like class that can be a non-type template parameter
  template<class K, class V, class Alloc = std::allocator<StructuralPair<K, V>>, class Comp = std::less<K>>
  class StructuralConstMap {
  public:
    using AllocTraits = std::allocator_traits<Alloc>;
    using cpointer = AllocTraits::const_pointer;
    using pointer = AllocTraits::pointer;
    typedef K K_t;
    typedef V V_t;
    typedef StructuralPair<K, V> T;
    typedef const StructuralPair<K, V>& cref;
    struct pairComp_t { bool operator()(const T& a, const T& b) const { return Comp()(a.k, b.k); }; };

  private:
    pairComp_t pairComp;
    Alloc alloc;
    const Alloc::size_type len;
    pointer data { nullptr };

  public:
    constexpr StructuralConstMap(const StructuralConstMap& o) = default;
    template<typename Iter> constexpr StructuralConstMap(const Iter first, const Iter last) :
      len(std::distance(first, last)),
      data(const_copy(first, last, AllocTraits::allocate(alloc, len), alloc))
    {
      std::sort(data, data + len);
    };
    constexpr StructuralConstMap(const std::initializer_list<T> data) : StructuralConstMap(data.begin(), data.end()) {};
    constexpr ~StructuralConstMap() {
      AllocTraits::deallocate(alloc, data, len);
    };
    constexpr inline auto count() const { return len; };
    constexpr const V& operator[](const K& k) const { return *std::lower_bound(data, data+len, k, pairComp); };
    constexpr inline bool contains(const K& k) const { return std::binary_search(data, data+len, k, pairComp); };
  };

}
