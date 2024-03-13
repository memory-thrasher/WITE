#pragma once

#include <vector>
#include <algorithm>

namespace WITE {

  //only useful/intended as a temporary in a consteval function, for runtime use std::map (which cannot be used constexpr)
  template<class K, class V> struct tempMap {
    typedef std::pair<K, V> T;
    std::vector<T> data;
    constexpr V& operator[](const K& k) {
      for(T& t : data)
	if(t.first == k)
	  return t.second;
      return data.emplace_back(k, V()).second;
    };
    constexpr bool contains(const K& k) const {
      for(const T& t : data)
	if(t.first == k)
	  return true;
      return false;
    };
  };

};
