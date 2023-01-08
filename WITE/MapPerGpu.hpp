#pragma once

#include <map>

#include "StructuralPair.hpp"

namespace WITE::GPU {

  template<class K, class V> class PerGpu<std::map<K, V>> {
  private:
    typedef Collections::StructuralPair<size_t, K> pair;
    std::map<pair, V> data;
  public:
    PerGpu() {};
    inline V& get(size_t t, const K& k) { return data[{t, k}]; };
    inline bool contains(size_t t, const K& k) { return data.contains({t, k}); };
  };

};
