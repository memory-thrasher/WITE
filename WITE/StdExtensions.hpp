#pragma once

#include <vector>
#include <algorithm>
#include <cstring> //memcpy

namespace WITE::Collections {

  template<typename T> inline void remove(std::vector<T>& t, T p) {
    t.erase(std::remove(t.begin(), t.end(), p), t.end());
  }

  template<typename T, class UP> inline void remove_if(std::vector<T>& t, UP p) {
    t.erase(std::remove_if(t.begin(), t.end(), p), t.end());
  }

  template<typename T, class C> inline constexpr bool contains(C& t, T e) {
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

  template<typename D, typename S> inline void memcpy(D* dst, const S* src, size_t len)
    requires std::negation_v<std::is_volatile<D>> {
    std::memcpy(reinterpret_cast<void*>(dst), reinterpret_cast<const void*>(src), len);
  }

  template<typename D, typename S> inline void memcpy(D* dst, const S* src, size_t len)
    requires std::is_volatile_v<D> {
    for(size_t i = len;i;) {
      --i;
      dst[i] = src[i];
    }
  }

  template<typename D, typename S> inline auto memcmp(const D* dst, const S* src, size_t len) {
    return std::memcmp(reinterpret_cast<const void*>(dst), reinterpret_cast<const void*>(src), len);
  }

  template<typename T> inline void memset(T* dst, const char value, const size_t len) {
    std::memset(reinterpret_cast<void*>(dst), value, len);
  }

  // template<typename CVT, typename T = std::remove_cv_t<CVT>> inline T cv_cast(CVT i) {
  //   return const_cast<T>(i);
  // }

  template<typename CVT, typename T = std::remove_cv_t<CVT>> inline T* cv_remove(CVT* i) {
    return const_cast<T*>(i);
  }

  template<typename CVT, typename T = std::remove_cv_t<CVT>>
  inline T&& cv_remove(CVT i) requires std::is_trivially_copyable_v<T> {
    T ret = i;
    return std::move(ret);
  }

  template<typename T> [[nodiscard]] inline constexpr T* calloc(size_t cnt) {
    std::allocator<T> a;
    return std::allocator_traits<std::allocator<T>>::allocate(a, cnt);
    //return reinterpret_cast<T*>(std::malloc(sizeof(T) * cnt));
  }

  //I and O are either iterators or pointers
  template<class I, class O, class Alloc> constexpr O const_copy(I start, I end, O out, Alloc& alloc) {
    while(start != end)
      std::allocator_traits<Alloc>::construct(alloc, &*out, *start++);//constructor or assignment call
    return out;
  }

}
