#pragma once

#include <initializer_list>
#include <array>

#include "CopyableArray.hpp"

namespace WITE::Collections {

  template<class T> constexpr size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

#define defineLiteralList(T, NOM, ...) constexpr ::WITE::Collections::CopyableArray<T, ::WITE::Collections::countIL<T>({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

  template<typename T> struct LiteralList {
    const T* data;
    size_t len;
    constexpr size_t count() const { return len; };
    constexpr LiteralList() = default;
    constexpr LiteralList(const LiteralList<T>& o) = default;
    constexpr LiteralList(const T* data, const size_t len) : data(data), len(len) {};
    template<size_t LEN> constexpr LiteralList(const Collections::CopyableArray<T, LEN>& o) : LiteralList(o.data(), LEN) {};
    template<size_t LEN> constexpr LiteralList(const T (&o)[LEN]) : LiteralList(o, LEN) {};
    constexpr ~LiteralList() = default;
    constexpr inline auto begin() const { return data; };
    constexpr inline auto end() const { return data+len; };
    constexpr inline const T& operator[](int i) const { return data[i]; };
    constexpr inline const LiteralList sub(size_t start, size_t len) const { return { data+start, len }; };

    template<class L> constexpr inline const size_t countWhere(L l) const {
      size_t ret = 0;
      for(size_t i = 0;i < len;i++)
	if(l(data[i]))
	  ret++;
      return ret;
    };

    template<class L> consteval inline const auto where(L l) const {
      //TODO see if the size can be inlined if constexpr is changed to consteval
      constexpr size_t C = countWhere(l);
      size_t j = 0;
      T ret[C];
      for(size_t i = 0;i < C && i < C;i++)
	if(l(data[i]))
	  ret[j++] = data[i];
      return ret;
    };

  };

  // template<class T, LiteralList<T> LL, class L> constexpr static inline const auto where(L l) {
  //   return LL.where<LL.countWhere(l)>(l);
  // };

  template<size_t layers, class T> constexpr Collections::CopyableArray<size_t, 1> countIlRecursive(const std::initializer_list<T> il) {
    return { countIL(il) };
  };

  template<size_t layers, class T> constexpr Collections::CopyableArray<size_t, layers> countIlRecursive(const std::initializer_list<std::initializer_list<T>> il) {
    Collections::CopyableArray<size_t, layers> ret = {};
    for(const auto il2 : il) {
      Collections::CopyableArray<size_t, layers-1> temp = countIlRecursive<layers-1>(il2);
      for(size_t i = 1;i < layers;i++)
	ret[i] += temp[i-1];
      ret[0]++;
    }
    return ret;
  };

  template<size_t start, size_t len, class T, size_t srcLen> requires (start + len <= srcLen)
  constexpr Collections::CopyableArray<T, len> subArray(const Collections::CopyableArray<T, srcLen> src) {
    // if constexpr(len <= 0)
    //   return {};
    // else if(len == 1)
    //   return { stc[start] };
    // else if(len == 2)
    //   return { src[start], src[start+1] };
    // else
    //   return { src[start], std::forward(subArray<start+1, len-1, T, srcLen>(src) };
    //^^maybe this?
    Collections::CopyableArray<T, len> ret = {};
    for(size_t i = 0;i < len;i++)
      ret[i] = src[start+i];
    return ret;
  };

  // template<size_t start, size_t len, class T>
  // constexpr Collections::CopyableArray<T, len> subArray(const LiteralList<T> src) {
  //   if constexpr(len <= 0)
  //     return {};
  //   else if constexpr(len == 1)
  //     return { src[start] };
  //   else if constexpr(len == 2)
  //     return { src[start], src[start+1] };
  //   else
  //     return { src[start], std::forward(subArray<start+1, len-1, T>(src)) };
  //   // Collections::CopyableArray<T, len> ret;
  //   // for(size_t i = 0;i < len;i++)
  //   //   ret[i] = src[start+i];
  //   // return ret;
  // };

  template<size_t N> constexpr size_t sum(Collections::CopyableArray<size_t, N> src) {
    size_t ret = 0;
    for(size_t v : src)
      ret += v;
    return ret;
  };

  template<class V, size_t N> constexpr Collections::CopyableArray<V, N+1> cumulativeSum(const Collections::CopyableArray<V, N>& src) {
    Collections::CopyableArray<V, N+1> ret;
    V running {};
    for(size_t i = 0;i < N;i++) {
      ret[i] = running;
      running += src[i];
    }
    ret[N] = running;
    return ret;
  };

  template<class V, size_t N, size_t M>
  constexpr Collections::CopyableArray<V, N+M> concatArray(const Collections::CopyableArray<V, N>& a, const Collections::CopyableArray<V, M>& b) {
    Collections::CopyableArray<V, N+M> ret;
    std::copy(a.begin(), a.end(), ret.begin());
    std::copy(b.begin(), b.end(), ret.begin() + N);
    return ret;
  };

  template<class... args> constexpr auto tie(args... in) {
    constexpr size_t len = sizeof...(in);
    typedef typename std::common_type<args...>::type ct;
    Collections::CopyableArray<ct, len> pa { std::forward<ct>(in)... };
    return pa;
  };

  template<class T, class... args> constexpr auto tie_cast(args... in) {
    constexpr size_t len = sizeof...(in);
    Collections::CopyableArray<T, len> pa { std::forward<T>(in)... };
    return pa;
  };

  template<class T> struct remove_initializer_list {
    typedef T type;
  };

  template<class U> struct remove_initializer_list<std::initializer_list<U>> {
    typedef remove_initializer_list<U>::type type;
  };

  template<class T> struct count_initializer_list_layers {
    static constexpr size_t value = 0;
  };

  template<class U> struct count_initializer_list_layers<std::initializer_list<U>> {
    static constexpr size_t value = count_initializer_list_layers<U>::value + 1;
  };

};
