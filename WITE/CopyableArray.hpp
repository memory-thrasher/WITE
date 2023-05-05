#pragma once

#include "Concepts.hpp"

namespace WITE::Collections {

  template<typename T, size_t LEN> struct CopyableArray {
    template<typename L> struct Generator {
      L l;
      constexpr Generator(const L l) : l(l) {};
      constexpr inline auto operator[](size_t i) const { return l(i); };
    };//so parameter packs can be expanded to invoke l a number of times

    template<class I> struct Uniterator {
      const I begin;
      constexpr Uniterator(const I i) : begin(i) {};
      constexpr inline auto operator[](size_t i) { I v = begin; while(i--) ++v; return *v; };
    };//so parameter packs can be expanded to fetch the nth value from an iterator (slow; intended for consteval only)

    std::array<T, LEN> data;

    constexpr CopyableArray() = default;
    template<indexable_to<T> S, size_t... I> constexpr CopyableArray(const S s, std::index_sequence<I...>) : data({ s[I]... }) {};
    template<indexable_to<T> S> constexpr CopyableArray(const S s) : CopyableArray(s, std::make_index_sequence<LEN>()) {};
    template<lambda_indexer_to<T> L> constexpr CopyableArray(const L l) : CopyableArray(Generator<L>(l)) {};
    template<iterator_not_ptr<T> I> constexpr CopyableArray(const I begin) : CopyableArray(Uniterator(begin)) {};//end ignored
    template<iterable_unindexable<T> C> constexpr CopyableArray(const C c) : CopyableArray(c.begin(), c.end()) {};
    template<iterable_unindexable<T> C> constexpr CopyableArray(const C& c) : CopyableArray(c.begin(), c.end()) {};//vector
    constexpr CopyableArray(const std::initializer_list<T> il) : CopyableArray(il.begin()) {};

    constexpr inline T& operator[](size_t i) { return data[i]; };
    constexpr inline const T& operator[](size_t i) const { return data[i]; };
    constexpr inline explicit operator const T*() const { return data.data(); };
    constexpr inline explicit operator T*() { return data.data(); };

    constexpr inline auto begin() { return data.begin(); };
    constexpr inline auto begin() const { return data.begin(); };
    constexpr inline auto end() { return data.end(); };
    constexpr inline auto end() const { return data.end(); };

  };

};
