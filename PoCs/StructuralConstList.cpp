/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

// #include <initializer_list>
// #include <memory>

// template<class T, class Alloc = std::allocator<T>> class StructuralConstList {
// public:
//   using AllocTraits = std::allocator_traits<Alloc>;
//   Alloc alloc;
//   const Alloc::size_type len;
//   AllocTraits::pointer data;
//   constexpr StructuralConstList(std::initializer_list<T> list) :
//     len(std::distance(list.begin(), list.end())),
//     data(AllocTraits::allocate(alloc, len))
//   {
//     auto start = list.begin();
//     while(start != list.end())
//       std::allocator_traits<Alloc>::construct(alloc, data, *start++);
//   };
//   constexpr ~StructuralConstList() {
//     AllocTraits::deallocate(alloc, data, len);
//   };
//   constexpr inline auto begin() const { return data; };
//   constexpr inline auto end() const { return data + len; };
// };

  //  static constexpr StructuralConstList<int> ints = { 7, 12, 49, 157 };

#include <vector>
#include <algorithm>

constexpr int f() {
    std::vector<int> v = {1, 2, 3};
    return v.size();
}

static_assert(f() == 3);

int main(int argc, char** argv) {
  return 0;
}


