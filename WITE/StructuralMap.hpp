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
    constexpr StructuralMap() = default;
    constexpr ~StructuralMap() = default;
    V& operator[](const K& k) {
      auto locationIter = std::lower_bound(data.cbegin(), data.cend());
      if(locationIter == data.cend() || !equals(*locationIter.k, k))
	data.emplace(locationIter, k);
      return *locationIter;
    };
    constexpr auto begin() const { return data.cbegin(); };
    constexpr auto end() const { return data.cend(); };
    constexpr auto size() const { return data.size(); };
    void clear() { data.clear(); };
  };

}
