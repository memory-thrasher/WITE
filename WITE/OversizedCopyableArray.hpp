#pragma once\

#include <type_traits>

#include "Concepts.hpp"

namespace WITE {

  template<class T, size_t MAX_LEN> requires std::is_default_constructible_v<T> struct oversizedCopyableArray {

    std::array<T, MAX_LEN> data;
    size_t len = 0;

    constexpr OversizedCopyableArray() = default;

    template<indexable_to<T> S, size_t... I> constexpr OversizedCopyableArray(const S s, std::index_sequence<I...>) : data({ s[I]... }) {
      fillWithDefault();
    };

    template<class S> constexpr OversizedCopyableArray(const S s, size_t len) : len(len) {
      for(size_t i = 0;i < len;i++)
	data[i] = s[i];
      fillWithDefault();
    };

    template<lambda_indexer_to<T> L> constexpr OversizedCopyableArray(const L l, size_t len) : len(len) {
      for(size_t i = 0;i < len;i++)
	data[i] = l(i);
      fillWithDefault();
    };

    template<iterator<T> I> constexpr OversizedCopyableArray(const I begin, const I end) {
      len = 0;
      I i = begin;
      while(i != end)
	data[len++] = *(i++);
      fillWithDefault();
    };

    template<class C> requires std::is_trivially_constructible_v<C> constexpr OversizedCopyableArray(const C c) :
      OversizedCopyableArray(c.begin(), c.end()) {};

    template<class C> requires (!std::is_trivially_constructible_v<C>) constexpr OversizedCopyableArray(const C& c) :
      OversizedCopyableArray(c.begin(), c.end()) {};

    constexpr OversizedCopyableArray(const std::initializer_list<T> il) : OversizedCopyableArray(il.begin(), il.end()) {};

    constexpr void fillWithDefault() {
      for(size_t i = len;i < MAX_LEN;i++)
	data[i] = T{};
    };

    constexpr inline T& operator[](size_t i) { return data[i]; };
    constexpr inline const T& operator[](size_t i) const { return data[i]; };
    constexpr inline explicit operator const T*() const { return data.data(); };
    constexpr inline explicit operator T*() { return data.data(); };
    constexpr inline T* ptr() { return data.data(); };
    constexpr inline const T* ptr() const { return data.data(); };

    constexpr inline auto begin() { return data.begin(); };
    constexpr inline auto begin() const { return data.begin(); };
    constexpr inline auto end() { return data.begin() + len; };
    constexpr inline auto end() const { return data.begin() + len; };

  };

};
