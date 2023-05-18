#pragma once

#include "Concepts.hpp"

namespace WITE::Collections {

  /*
    array-like object that can be returned by a constexpr function that does NOT require T to be default-constructible.
    T must be copy-constructible
   */
  template<typename T, size_t LEN> struct CopyableArray {
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

    static constexpr size_t LENGTH = LEN;

    std::array<T, LEN> data;

    constexpr CopyableArray() = default;
    template<indexable_to<T> S, size_t... I> constexpr CopyableArray(const S s, std::index_sequence<I...>) : data({ s[I]... }) {};
    template<indexable_to<T> S> constexpr CopyableArray(const S s) : CopyableArray(s, std::make_index_sequence<LEN>()) {};
    template<lambda_indexer_to<T> L> consteval CopyableArray(const L l) : CopyableArray(Generator<L>(l)) {};
    template<iterator_not_ptr<T> I> constexpr CopyableArray(const I begin) : CopyableArray(Uniterator(begin)) {};//end ignored
    template<iterable_unindexable<T> C> constexpr CopyableArray(const C c) : CopyableArray(c.begin(), c.end()) {};
    template<iterable_unindexable<T> C> constexpr CopyableArray(const C& c) : CopyableArray(c.begin(), c.end()) {};//vector
    constexpr CopyableArray(const std::initializer_list<T> il) : CopyableArray(il.begin()) {};

    //L2 below somehow prevents this constructor being considered for the copy constructor with default initialized T
    template<size_t L2 = LEN-1, class... Args> requires (LEN > 1, L2 > 0)
      consteval CopyableArray(CopyableArray<T, L2>&& firstBit, Args... nextBit) :
		 CopyableArray([&firstBit, nextBit...](size_t i)constexpr{
		   if(i < LEN-1) return std::move(firstBit[i]);
		   else return T{nextBit...};
		 }) {};//recursion aid
    template<size_t L2 = LEN-1, class... Args> requires (LEN > 1, L2 > 0)
      consteval CopyableArray(const CopyableArray<T, L2>& firstBit, Args... nextBit) :
		 CopyableArray([&firstBit, nextBit...](size_t i)constexpr{
		   if(i < LEN-1) return firstBit[i];
		   else return T{nextBit...};
		 }) {};//recursion aid
    template<class... Args> requires (LEN == 1) constexpr CopyableArray(Args... nextBit) : data({{nextBit...}}) {};

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

  };

};
