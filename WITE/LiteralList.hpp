#pragma once

#include <initializer_list>
#include <array>

#include "ConstevalArray.hpp"

namespace WITE::Collections {

  template<class T> consteval size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

#define defineLiteralList(T, NOM, ...) constexpr ::WITE::Collections::ConstevalArray<T, ::WITE::Collections::countIL<T>({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

  template<typename T> struct LiteralList {
    const ConstevalArrayBase<T>* data;
    size_t len;//maybe < data->length() if this is a sub-list
    constexpr size_t count() const { return len; };
    constexpr LiteralList() = default;
    constexpr LiteralList(const LiteralList<T>& o) = default;
    template<size_t LEN> consteval LiteralList(const ConstevalArray<T, LEN>& o) : data(&o), len(LEN) {};
    consteval LiteralList(const ConstevalArrayBase<T>& o, size_t len) : data(&o), len(len) {};
    consteval LiteralList(const ConstevalArrayBase<T>& o) : data(&o), len(o.length()) {};
    constexpr ~LiteralList() = default;
    consteval inline const T& operator[](size_t i) const { return data->operator[](i); };//runtime querying should use an iterator of a staged array
    constexpr inline auto begin() const { return data->begin(); };
    constexpr inline decltype(data->end()) end() const {
      if(len == 0) return {};
      if(len == data->length()) return data->end();
      auto ret = data->begin();
      std::advance(ret, len);
      return ret;
    };

    template<class L> consteval inline const size_t countWhere(L l) const {
      size_t ret = 0;
      for(size_t i = 0;i < len;i++)
	if(l(data[i]))
	  ret++;
      return ret;
    };

    template<class L> consteval inline const auto where(L l) const {
      constexpr size_t C = countWhere(l);
      size_t j = 0;
      ConstevalArray<T, C> ret;
      for(size_t i = 0;i < C;i++)
	if(l(data[i]))
	  ret[j++] = data[i];
      return ret;
    };

    consteval inline const auto sub(size_t skip) const {
      return sub(skip, len-skip);
    };

    consteval inline const auto sub(size_t skip, size_t keep) const {
      ASSERT_CONSTEXPR(skip + keep <= len);//sub list out of bounds
      auto d = data;
      while(skip--) d = d->next();
      return LiteralList<T>(*d, keep);
    };

  };

  template<class T, LiteralList<T> L, size_t... I> consteval std::array<T, L.len> toStdArray() {
    return {{ L[I]... }};
  };

  template<class T, LiteralList<T> L> consteval std::array<T, L.len> toStdArray() {
    return toStdArrayImpl<T, L>(std::make_index_sequence<L.len>{});
  };

  // template<class T, LiteralList<T> LL, class L> consteval static inline const auto where(L l) {
  //   return LL.where<LL.countWhere(l)>(l);
  // };

  template<size_t layers, class T> consteval Collections::ConstevalArray<size_t, 1> countIlRecursive(const std::initializer_list<T> il) {
    return { countIL(il) };
  };

  template<size_t layers, class T> consteval Collections::ConstevalArray<size_t, layers> countIlRecursive(const std::initializer_list<std::initializer_list<T>> il) {
    Collections::ConstevalArray<size_t, layers> ret = {};
    for(const auto il2 : il) {
      Collections::ConstevalArray<size_t, layers-1> temp = countIlRecursive<layers-1>(il2);
      for(size_t i = 1;i < layers;i++)
	ret[i] += temp[i-1];
      ret[0]++;
    }
    return ret;
  };

  template<size_t start, size_t len, class T, size_t srcLen> requires (start + len <= srcLen)
  consteval Collections::ConstevalArray<T, len> subArray(const Collections::ConstevalArray<T, srcLen> src) {
    // if constexpr(len <= 0)
    //   return {};
    // else if(len == 1)
    //   return { stc[start] };
    // else if(len == 2)
    //   return { src[start], src[start+1] };
    // else
    //   return { src[start], std::forward(subArray<start+1, len-1, T, srcLen>(src) };
    //^^maybe this?
    Collections::ConstevalArray<T, len> ret = {};
    for(size_t i = 0;i < len;i++)
      ret[i] = src[start+i];
    return ret;
  };

  // template<size_t start, size_t len, class T>
  // constexpr Collections::ConstevalArray<T, len> subArray(const LiteralList<T> src) {
  //   if constexpr(len <= 0)
  //     return {};
  //   else if constexpr(len == 1)
  //     return { src[start] };
  //   else if constexpr(len == 2)
  //     return { src[start], src[start+1] };
  //   else
  //     return { src[start], std::forward(subArray<start+1, len-1, T>(src)) };
  //   // Collections::ConstevalArray<T, len> ret;
  //   // for(size_t i = 0;i < len;i++)
  //   //   ret[i] = src[start+i];
  //   // return ret;
  // };

  template<size_t N> consteval size_t sum(Collections::ConstevalArray<size_t, N> src) {
    size_t ret = 0;
    for(size_t v : src)
      ret += v;
    return ret;
  };

  template<class V, size_t N> consteval Collections::ConstevalArray<V, N+1> cumulativeSum(const Collections::ConstevalArray<V, N>& src) {
    Collections::ConstevalArray<V, N+1> ret;
    V running {};
    for(size_t i = 0;i < N;i++) {
      ret[i] = running;
      running += src[i];
    }
    ret[N] = running;
    return ret;
  };

  template<class V, size_t N, size_t M>
  consteval Collections::ConstevalArray<V, N+M> concatArray(const Collections::ConstevalArray<V, N>& a, const Collections::ConstevalArray<V, M>& b) {
    Collections::ConstevalArray<V, N+M> ret;
    std::copy(a.begin(), a.end(), ret.begin());
    std::copy(b.begin(), b.end(), ret.begin() + N);
    return ret;
  };

  template<class... args> consteval auto tie(args... in) {
    constexpr size_t len = sizeof...(in);
    typedef typename std::common_type<args...>::type ct;
    Collections::ConstevalArray<ct, len> pa { std::forward<ct>(in)... };
    return pa;
  };

  template<class T, class... args> consteval auto tie_cast(args... in) {
    constexpr size_t len = sizeof...(in);
    Collections::ConstevalArray<T, len> pa { std::forward<T>(in)... };
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
