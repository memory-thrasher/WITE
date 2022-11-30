#pragma once

#include <map>

namespace WITE::GPU {

  template<class K, class V> class PerGpu<std::map<K, V>> {
  private:
    std::map<K, V> data[MAX_GPUS];
    //TODO destroyer?
  public:
    PerGpu() {};
    inline V& get(size_t t, const K& k) { return data[t][k]; };
    inline bool contains(size_t t, const K& k) { return data[t].contains(k); };
  };

};
