#pragma once

#include <initializer_list>
#include <array>
#include <ranges>

#include "copyableArray.hpp"

namespace WITE {

  template<class T> consteval size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

#define defineLiteralListScalar(T, NOM, ...) constexpr ::WITE::copyableArray<T, ::WITE::countIL<T>({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

#define defineLiteralList(T, NOM, ...) constexpr ::WITE::copyableArray<T, ::WITE::countIL<T>({ __VA_ARGS__ })> NOM = {{ __VA_ARGS__ }}

  template<typename T> struct literalList {
    const T* data;
    size_t len;//maybe < data->length() if this is a sub-list

    constexpr size_t count() const { return len; };
    constexpr literalList() : data(NULL), len(0) {};
    constexpr literalList(const literalList<T>& o) = default;
    constexpr literalList(const T& o) : data(&o), len(1) {};
    constexpr literalList(const T* data, const size_t len) : data(data), len(len) {};
    template<size_t LEN> constexpr literalList(const copyableArray<T, LEN>& o, size_t len) : literalList((const T*)o, len) {};
    template<size_t LEN> constexpr literalList(const copyableArray<T, LEN>& o) : literalList((const T*)o, LEN) {};
    template<size_t LEN> constexpr literalList(const std::array<T, LEN>& o) : literalList(o.data(), LEN) {};
    template<size_t LEN> constexpr literalList(const T (&o)[LEN]) : literalList(o, LEN) {};
    constexpr ~literalList() = default;

    constexpr inline auto begin() const { return data; };
    constexpr inline auto end() const { return data+len; };
    constexpr inline const T& operator[](int i) const { return data[i]; };
    consteval inline const literalList sub(size_t start, size_t len) const { return { data+start, len }; };
    consteval inline const literalList sub(size_t start) const { return { data+start, len-start }; };
    constexpr operator bool() const { return len; };

    template<typename P> constexpr inline std::vector<T> where(P p) const {
      std::vector<T> ret;
      for(size_t i = 0;i < len;i++)
	if(p(data[i]))
	  ret.push_back(data[i]);
      return ret;
    };

    template<typename P> consteval inline const T* firstWhere(P p) const {
      for(size_t i = 0;i < len;i++)
	if(p(data[i]))
	  return &data[i];
      return NULL;
    };

    template<typename P> consteval inline size_t countWhereCE(P p) const {
      size_t ret = 0;
      for(size_t i = 0;i < len;i++)
	if(p(data[i]))
	  ret++;
      return ret;
    };

    template<typename P> constexpr inline size_t countWhere(P p) const {
      size_t ret = 0;
      for(size_t i = 0;i < len;i++)
	if(p(data[i]))
	  ret++;
      return ret;
    };

    inline constexpr bool contains(T e) const {
      for(auto v : *this)
	if(v == e)
	  return true;
      return false;
    };

  };

  template<class T, literalList<T> LL, class P> constexpr copyableArray<T, LL.countWhere(P())> where() {
    return { LL.where(P()) };
  };

  template<class T, class U, literalList<T> L, class P> consteval copyableArray<U, L.len> map(P p) {
    return [p](size_t i)->U { return p(L[i]); };
  };

  template<class T, literalList<T> L, size_t... I> consteval std::array<T, L.len> toStdArray() {
    return {{ L[I]... }};
  };

  template<class T, literalList<T> L> consteval std::array<T, L.len> toStdArray() {
    return toStdArrayImpl<T, L>(std::make_index_sequence<L.len>{});
  };

  template<size_t layers, class T> consteval copyableArray<size_t, 1> countIlRecursive(const std::initializer_list<T> il) {
    return { countIL(il) };
  };

  template<size_t layers, class T> consteval copyableArray<size_t, layers> countIlRecursive(const std::initializer_list<std::initializer_list<T>> il) {
    copyableArray<size_t, layers> ret = {};
    for(const auto il2 : il) {
      copyableArray<size_t, layers-1> temp = countIlRecursive<layers-1>(il2);
      for(size_t i = 1;i < layers;i++)
	ret[i] += temp[i-1];
      ret[0]++;
    }
    return ret;
  };

  template<size_t start, size_t len, class T, size_t srcLen> requires (start + len <= srcLen)
  consteval copyableArray<T, len> subArray(const copyableArray<T, srcLen> src) {
    // if constexpr(len <= 0)
    //   return {};
    // else if(len == 1)
    //   return { stc[start] };
    // else if(len == 2)
    //   return { src[start], src[start+1] };
    // else
    //   return { src[start], std::forward(subArray<start+1, len-1, T, srcLen>(src) };
    //MAYBE ^^
    copyableArray<T, len> ret = {};
    for(size_t i = 0;i < len;i++)
      ret[i] = src[start+i];
    return ret;
  };

  // template<size_t start, size_t len, class T>
  // constexpr copyableArray<T, len> subArray(const literalList<T> src) {
  //   if constexpr(len <= 0)
  //     return {};
  //   else if constexpr(len == 1)
  //     return { src[start] };
  //   else if constexpr(len == 2)
  //     return { src[start], src[start+1] };
  //   else
  //     return { src[start], std::forward(subArray<start+1, len-1, T>(src)) };
  //   // copyableArray<T, len> ret;
  //   // for(size_t i = 0;i < len;i++)
  //   //   ret[i] = src[start+i];
  //   // return ret;
  // };

  template<size_t N> consteval size_t sum(copyableArray<size_t, N> src) {
    size_t ret = 0;
    for(size_t v : src)
      ret += v;
    return ret;
  };

  template<class V, size_t N> consteval copyableArray<V, N+1> cumulativeSum(const copyableArray<V, N>& src) {
    copyableArray<V, N+1> ret;
    V running {};
    for(size_t i = 0;i < N;i++) {
      ret[i] = running;
      running += src[i];
    }
    ret[N] = running;
    return ret;
  };

  template<class... args> consteval auto tie(args... in) {
    constexpr size_t len = sizeof...(in);
    typedef typename std::common_type<args...>::type ct;
    copyableArray<ct, len> pa { std::forward<ct>(in)... };
    return pa;
  };

  template<class T, class... args> consteval auto tie_cast(args... in) {
    constexpr size_t len = sizeof...(in);
    copyableArray<T, len> pa { std::forward<T>(in)... };
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

  template<class V, literalList<V> L, literalList<V> R>
  consteval copyableArray<V, L.len+R.len> concat() {
    return [](size_t i) {
      return i < L.len ? L[i] : R[i - L.len];
    };
  };

  template<class V, literalList<V> L, literalList<V> R, literalList<V> Y, literalList<V>... Z>
  consteval auto concat() {
    static constexpr auto A = concat<L, R>();
    return concat<A, Y, Z...>();
  };

};
