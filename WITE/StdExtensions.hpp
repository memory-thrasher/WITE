#pragma once

#include <vector>
#include <algorithm>
#include <cstring> //memcpy

namespace WITE::Collections {

  template<typename T, class UP> inline void remove_if(std::vector<T>& t, UP p) {
    t.erase(std::remove_if(t.begin(), t.end(), p), t.end());
  }

  template<typename T, class C> inline bool contains(C& t, T e) {
    return std::find(t.begin(), t.end(), e) != t.end();
  }

  template<typename T, class C> inline void concat_move(C& dst, C& src) {
    dst.reserve(dst.size() + src.size());
    for(auto i = src.begin();i != src.end();i++)
      dst.push_back(std::move(*i));
    src.clear();
  }

  template<class T, class U, class V> void uniq(T src, U p, V& out) {
    for(auto it = src.begin();it != src.end();it++) {
      auto u = p(*it);
      if(!contains(out, u))
	out.push_back(u);
    }
  }

}

namespace WITE {

  template<typename D, typename S> inline void memcpy(D* dst, const S* src, size_t len) {
    std::memcpy(reinterpret_cast<void*>(dst), reinterpret_cast<const void*>(src), len);
  }

  template<typename D, typename S> inline auto memcmp(const D* dst, const S* src, size_t len) {
    return std::memcmp(reinterpret_cast<const void*>(dst), reinterpret_cast<const void*>(src), len);
  }

  template<typename T> inline void memset(T* dst, const char value, const size_t len) {
    std::memset(reinterpret_cast<void*>(dst), value, len);
  }

}
