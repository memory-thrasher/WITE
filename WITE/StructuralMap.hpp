#include <vector>
#include <algorithm>

#include "StructuralPair.hpp"

namespace WITE::Collections {

  template<class K, class V, class Alloc = std::allocator<StructuralPair<K, V>>, class Comp = std::less<K>>
  class StructuralMap {
  private:
    using AllocTraits = std::allocator_traits<Alloc>;
    using cpointer = AllocTraits::const_pointer;
    using pointer = AllocTraits::pointer;
    typedef K K_t;
    typedef V V_t;
    typedef StructuralPair<K, V> T;
    typedef const StructuralPair<K, V>& cref;
    struct pairComp_t { bool operator()(const T& a, const T& b) const { return Comp()(a.k, b.k); }; };
    constexpr bool equals(const K& l, const K& r) { return !(l < r) && !(r < l); };
    std::vector<StructuralPair<K, V>, Alloc> data;

  public:
    constexpr StructuralMap(std::initializer_list<StructuralPair<K, V>> il) : data(il) {};
    constexpr StructuralMap() = default;
    constexpr ~StructuralMap() = default;
    constexpr V& operator[](const K& k) {
      auto locationIter = std::lower_bound(data.begin(), data.end(), k);
      if(locationIter == data.cend())
	return data.emplace_back(k).v;
      else if(!equals(locationIter->k, k))
	locationIter = data.emplace(locationIter, k);
      return locationIter->v;
    };
    constexpr auto begin() const { return data.cbegin(); };
    constexpr auto end() const { return data.cend(); };
    constexpr auto size() const { return data.size(); };
    constexpr void clear() { data.clear(); };
  };

}
