#pragma once

#include <memory>
#include <initializer_list>
#include <algorithm>

#include "StdExtensions.hpp"

namespace WITE::Collections {

  template<class T, class Alloc = std::allocator<T>> class StructuralConstList {
  public:
    using AllocTraits = std::allocator_traits<Alloc>;
    using cpointer = AllocTraits::const_pointer;
    using pointer = AllocTraits::pointer;
    typedef const T& cref;

    Alloc alloc;
    const Alloc::size_type len;
    pointer data { nullptr };
    constexpr StructuralConstList(const StructuralConstList& o) = default;
    template<typename Iter>
    constexpr StructuralConstList(Iter first, Iter last) :
      len(std::distance(first, last)),
      data(const_copy(first, last, AllocTraits::allocate(alloc, len), alloc)) {};
    constexpr StructuralConstList(const std::initializer_list<T> data) : StructuralConstList(data.begin(), data.end()) {};
    constexpr ~StructuralConstList() {
      AllocTraits::deallocate(alloc, const_cast<AllocTraits::pointer>(data), len);
    };
    constexpr inline cpointer begin() const { return data; };
    constexpr inline cpointer cbegin() const { return data; };
    constexpr inline cpointer end() const { return data + len; };
    constexpr inline cpointer cend() const { return data + len; };
    constexpr inline pointer begin() { return data; };
    constexpr inline pointer end() { return data + len; };
    constexpr inline cref operator[](Alloc::size_type len) const { return *(data + len); };
    constexpr inline bool contains(const T& t) const { return Collections::contains(*this, t); };
    template<class UP> constexpr inline bool contains(const UP up) const { return std::any_of(begin(), end(), up); };
    template<class UP> constexpr inline bool every(const UP up) const { return std::all_of(begin(), end(), up); };
    template<class V> constexpr inline bool intersectsMap(const std::map<T, V>& o) const {
      return std::any_of(begin(), end(), [o](T t) {
	std::pair<T, V> temp(t, {});
	return std::binary_search(o.begin(), o.end(), temp, o.value_comp());
      });
    };
  };

};
