#pragma once

#include <memory>
#include <initializer_list>
#include <algorithm>

namespace WITE::Collections {

  template<class T, class Alloc = std::allocator<T>> class StructuralConstList {
  public:
    using AllocTraits = std::allocator_traits<Alloc>;
    using cpointer = AllocTraits::const_pointer;
    typedef const T& cref;

    Alloc alloc;
    const Alloc::size_type len;
    cpointer data;
    constexpr StructuralConstList(const StructuralConstList& o) = default;
    template<typename Iter>
    constexpr StructuralConstList(Iter first, Iter last) :
      len(std::distance(first, last)),
      data(AllocTraits::allocate(alloc, len))
    {
      std::uninitialized_copy(first, last, data);
    };
    constexpr StructuralConstList(const std::initializer_list<T> data) : StructuralConstList(data.begin(), data.end()) {};
    constexpr ~StructuralConstList() {
      AllocTraits::deallocate(alloc, const_cast<AllocTraits::pointer>(data), len);
    };
    constexpr inline cpointer begin() const { return data; };
    constexpr inline cpointer cbegin() const { return data; };
    constexpr inline cpointer end() const { return data + len; };
    constexpr inline cpointer cend() const { return data + len; };
    constexpr inline cref operator[](Alloc::size_type len) const { return *(data + len); };
  };

};
