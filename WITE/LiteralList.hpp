#pragma once

#include <initializer_list>

namespace WITE::Collections {

  template<class T> constexpr size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

#define defineLiteralList(T, NOM, ...) constexpr ::std::array<T, ::WITE::Collections::countIL({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

  template<size_t layers, class T> constexpr std::array<size_t, 1> countIlRecursive(const std::initializer_list<T> il) {
    return { countIL(il) };
  };

  template<size_t layers, class T> constexpr std::array<size_t, layers> countIlRecursive(const std::initializer_list<std::initializer_list<T>> il) {
    std::array<size_t, layers> ret = {};
    for(const auto il2 : il) {
      std::array<size_t, layers-1> temp = countIlRecursive<layers-1>(il2);
      for(size_t i = 1;i < layers;i++)
	ret[i] += temp[i-1];
      ret[0]++;
    }
    return ret;
  };

  template<size_t start, size_t len, class T, size_t srcLen> requires (start + len <= srcLen)
    constexpr std::array<T, len> subArray(const std::array<T, srcLen> src) {
    std::array<T, len> ret = {};
    for(size_t i = 0;i < len;i++)
      ret[i] = src[start+i];
    return ret;
  };

  template<size_t N> constexpr size_t sum(std::array<size_t, N> src) {
    size_t ret = 0;
    for(size_t v : src)
      ret += v;
    return ret;
  };

  template<class V, size_t N> constexpr std::array<V, N+1> cumulativeSum(const std::array<V, N>& src) {
    std::array<V, N+1> ret;
    V running {};
    for(size_t i = 0;i < N;i++) {
      ret[i] = running;
      running += src[i];
    }
    ret[N] = running;
    return ret;
  };

  template<class V, size_t N, size_t M>
  constexpr std::array<V, N+M> concatArray(const std::array<V, N>& a, const std::array<V, M>& b) {
    std::array<V, N+M> ret;
    std::copy(a.begin(), a.end(), ret.begin());
    std::copy(b.begin(), b.end(), ret.begin() + N);
    return ret;
  };

  template<class... args> constexpr auto tie(args... in) {
    constexpr size_t len = sizeof...(in);
    typedef typename std::common_type<args...>::type ct;
    std::array<ct, len> pa { std::forward<ct>(in)... };
    return pa;
  };

  template<class T, class... args> constexpr auto tie_cast(args... in) {
    constexpr size_t len = sizeof...(in);
    std::array<T, len> pa { std::forward<T>(in)... };
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
