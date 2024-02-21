#pragma once

#include <vector>
#include <algorithm>

namespace WITE {

  //only useful as a temporary in a constexpr function, for runtime use std::map
  template<class K, class V> struct tempMap {
    typedef std::pair<K, V> T;
    std::vector<T> data;
    V& operator[](const K& k) {
      for(T& t : data)
	if(t.first == k)
	  return t.second;
      return data.emplace_back(k, V()).second;
    };
    bool contains(const K& k) {
      for(T& t : data)
	if(t.first == k)
	  return true;
      return false;
    };
  };

};
