#pragma once

#include "Concepts.hpp"

namespace WITE {

  /*
    array-like object that can be returned by a constexpr function that does NOT require T to be default-constructible.
    T must be copy-constructible
   */
  template<typename T, size_t LEN> struct alignas(T) copyableArray {
    template<typename L> struct Generator {
      L l;
      constexpr Generator(const L l) : l(l) {};
      consteval inline auto operator[](size_t i) const { return this->l(i); };
    };//so parameter packs can be expanded to invoke l a number of times

    template<class I> struct Uniterator {
      const I begin;
      constexpr Uniterator(const I i) : begin(i) {};
      constexpr inline auto operator[](size_t i) { I v = begin; while(i--) ++v; return *v; };
    };//so parameter packs can be expanded to fetch the nth value from an iterator (slow; intended for consteval only)

    template<class... Args> struct Factory {
      const std::tuple<Args...> args;
      constexpr Factory(std::tuple<Args...> args) : args(args) {};
      constexpr inline T operator[](size_t) const { return std::make_from_tuple<T>(args); };
    };//makes an instance for each call to operator with the same arguments

    static constexpr size_t LENGTH = LEN;

    alignas(T) std::array<T, LEN> data;

    constexpr copyableArray() = default;
    template<indexable_to<T> S, size_t... I> constexpr copyableArray(const S s, std::index_sequence<I...>) : data({ s[I]... }) {};
    template<indexable_to<T> S> constexpr copyableArray(const S s) : copyableArray(s, std::make_index_sequence<LEN>()) {};
    template<lambda_indexer_to<T> L> consteval copyableArray(const L l) : copyableArray(Generator<L>(l)) {};
    template<iterator_not_ptr<T> I> constexpr copyableArray(const I begin) : copyableArray(Uniterator(begin)) {};//end ignored
    template<iterable_unindexable<T> C> constexpr copyableArray(const C c) : copyableArray(c.begin(), c.end()) {};
    template<iterable_unindexable<T> C> constexpr copyableArray(const C& c) : copyableArray(c.begin(), c.end()) {};//vector
    constexpr copyableArray(const std::initializer_list<T> il) : copyableArray(il.begin()) {};

    //often enough we want to create each element in-position with the same arguments
    template<class... Args> requires requires(Args... args) { T(std::forward<Args>(args)...); }
    constexpr copyableArray(Args... args) :
      copyableArray(Factory<Args...>(std::forward_as_tuple(args...))) {};

    //L2 below somehow prevents this constructor being considered for the copy constructor with default initialized T
    // template<size_t L2 = LEN-1, class... Args> requires (LEN > 1, L2 > 0)
    //   consteval copyableArray(copyableArray<T, L2>&& firstBit, Args... nextBit) :
    // 		 copyableArray([&firstBit, nextBit...](size_t i)constexpr{
    // 		   if(i < LEN-1) return std::move(firstBit[i]);
    // 		   else return T{nextBit...};
    // 		 }) {};//recursion aid
    // template<size_t L2 = LEN-1, class... Args> requires (LEN > 1, L2 > 0)
    //   consteval copyableArray(const copyableArray<T, L2>& firstBit, Args... nextBit) :
    // 		 copyableArray([&firstBit, nextBit...](size_t i)constexpr{
    // 		   if(i < LEN-1) return firstBit[i];
    // 		   else return T{nextBit...};
    // 		 }) {};//recursion aid
    // template<class... Args> requires (LEN == 1) constexpr copyableArray(Args... nextBit) : data({{nextBit...}}) {};

    constexpr inline T& operator[](size_t i) { return data[i]; };
    constexpr inline const T& operator[](size_t i) const { return data[i]; };
    constexpr inline explicit operator const T*() const { return data.data(); };
    constexpr inline explicit operator T*() { return data.data(); };
    constexpr inline T* ptr() { return data.data(); };
    constexpr inline const T* ptr() const { return data.data(); };
    constexpr inline const size_t count() const { return LEN; };

    constexpr inline auto begin() { return data.begin(); };
    constexpr inline auto begin() const { return data.begin(); };
    constexpr inline auto end() { return data.end(); };
    constexpr inline auto end() const { return data.end(); };

    template<size_t OLEN> constexpr copyableArray<T, LEN+OLEN> operator+(const copyableArray<T, OLEN>& o) const {
      return [this, o](size_t i){ return i < LEN ? data[i] : o.data[i - LEN]; };
    };

  };

  template<typename T> struct copyableArray<T, 0> {
  };

};
