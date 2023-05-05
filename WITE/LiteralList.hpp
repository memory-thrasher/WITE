#pragma once

#include <initializer_list>
#include <array>
#include <ranges>

#include "CopyableArray.hpp"

namespace WITE::Collections {

  template<class T> consteval size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

#define defineLiteralListScalar(T, NOM, ...) constexpr ::WITE::Collections::CopyableArray<T, ::WITE::Collections::countIL<T>({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

#define defineLiteralList(T, NOM, ...) constexpr ::WITE::Collections::CopyableArray<T, ::WITE::Collections::countIL<T>({ __VA_ARGS__ })> NOM = {{ __VA_ARGS__ }}

  template<typename T> struct LiteralList {
    const T* data;
    size_t len;//maybe < data->length() if this is a sub-list

    constexpr size_t count() const { return len; };
    constexpr LiteralList() = default;
    constexpr LiteralList(const LiteralList<T>& o) = default;
    consteval LiteralList(const T* data, const size_t len) : data(data), len(len) {};
    template<size_t LEN> consteval LiteralList(const CopyableArray<T, LEN>& o, size_t len) : LiteralList((const T*)o, len) {};
    template<size_t LEN> consteval LiteralList(const CopyableArray<T, LEN>& o) : LiteralList((const T*)o, LEN) {};
    template<size_t LEN> consteval LiteralList(const std::array<T, LEN>& o) : LiteralList(o.data(), LEN) {};
    template<size_t LEN> consteval LiteralList(const T (&o)[LEN]) : LiteralList(o, LEN) {};
    constexpr ~LiteralList() = default;

    constexpr inline auto begin() const { return data; };
    constexpr inline auto end() const { return data+len; };
    constexpr inline const T& operator[](int i) const { return data[i]; };
    consteval inline const LiteralList sub(size_t start, size_t len) const { return { data+start, len }; };
    consteval inline const LiteralList sub(size_t start) const { return { data+start, len-start }; };

    // template<typename P> constexpr inline auto where(P p) {
    //   return std::ranges::views::filter(data, p);
    // };

    template<typename P> constexpr inline size_t countWhere(P p) {
      size_t ret = 0;
      for(const T& t : data)
	if(p(t))
	  ret++;
      return ret;
    };

  };

  template<class T, LiteralList<T> LL, class P, P p, size_t cnt = LL.countWhere(p)> CopyableArray<T, cnt> where() {
    return { std::ranges::views::filter(std::ranges::views::counted(LL.data, LL.len)) };
  };

  template<class T, LiteralList<T> L, size_t... I> consteval std::array<T, L.len> toStdArray() {
    return {{ L[I]... }};
  };

  template<class T, LiteralList<T> L> consteval std::array<T, L.len> toStdArray() {
    return toStdArrayImpl<T, L>(std::make_index_sequence<L.len>{});
  };

  template<size_t layers, class T> consteval CopyableArray<size_t, 1> countIlRecursive(const std::initializer_list<T> il) {
    return { countIL(il) };
  };

  template<size_t layers, class T> consteval CopyableArray<size_t, layers> countIlRecursive(const std::initializer_list<std::initializer_list<T>> il) {
    CopyableArray<size_t, layers> ret = {};
    for(const auto il2 : il) {
      CopyableArray<size_t, layers-1> temp = countIlRecursive<layers-1>(il2);
      for(size_t i = 1;i < layers;i++)
	ret[i] += temp[i-1];
      ret[0]++;
    }
    return ret;
  };

  template<size_t start, size_t len, class T, size_t srcLen> requires (start + len <= srcLen)
  consteval CopyableArray<T, len> subArray(const CopyableArray<T, srcLen> src) {
    // if constexpr(len <= 0)
    //   return {};
    // else if(len == 1)
    //   return { stc[start] };
    // else if(len == 2)
    //   return { src[start], src[start+1] };
    // else
    //   return { src[start], std::forward(subArray<start+1, len-1, T, srcLen>(src) };
    //^^maybe this?
    CopyableArray<T, len> ret = {};
    for(size_t i = 0;i < len;i++)
      ret[i] = src[start+i];
    return ret;
  };

  // template<size_t start, size_t len, class T>
  // constexpr CopyableArray<T, len> subArray(const LiteralList<T> src) {
  //   if constexpr(len <= 0)
  //     return {};
  //   else if constexpr(len == 1)
  //     return { src[start] };
  //   else if constexpr(len == 2)
  //     return { src[start], src[start+1] };
  //   else
  //     return { src[start], std::forward(subArray<start+1, len-1, T>(src)) };
  //   // CopyableArray<T, len> ret;
  //   // for(size_t i = 0;i < len;i++)
  //   //   ret[i] = src[start+i];
  //   // return ret;
  // };

  template<size_t N> consteval size_t sum(CopyableArray<size_t, N> src) {
    size_t ret = 0;
    for(size_t v : src)
      ret += v;
    return ret;
  };

  template<class V, size_t N> consteval CopyableArray<V, N+1> cumulativeSum(const CopyableArray<V, N>& src) {
    CopyableArray<V, N+1> ret;
    V running {};
    for(size_t i = 0;i < N;i++) {
      ret[i] = running;
      running += src[i];
    }
    ret[N] = running;
    return ret;
  };

  template<class V, size_t N, size_t M>
  consteval CopyableArray<V, N+M> concatArray(const CopyableArray<V, N>& a, const CopyableArray<V, M>& b) {
    CopyableArray<V, N+M> ret;
    std::copy(a.begin(), a.end(), ret.begin());
    std::copy(b.begin(), b.end(), ret.begin() + N);
    return ret;
  };

  template<class... args> consteval auto tie(args... in) {
    constexpr size_t len = sizeof...(in);
    typedef typename std::common_type<args...>::type ct;
    CopyableArray<ct, len> pa { std::forward<ct>(in)... };
    return pa;
  };

  template<class T, class... args> consteval auto tie_cast(args... in) {
    constexpr size_t len = sizeof...(in);
    CopyableArray<T, len> pa { std::forward<T>(in)... };
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
