/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <array>
#include <memory>
#include <iostream>

template<class T> constexpr size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

template<typename T> struct LiteralList {
  const T* data;
  const size_t len;
  constexpr LiteralList() = default;
  constexpr LiteralList(const T* data, const size_t len) : data(data), len(len) {};
  template<size_t LEN> constexpr LiteralList(const std::array<T, LEN>& o) : LiteralList(o.data(), LEN) {};
  constexpr ~LiteralList() = default;
};

#define defineLiteralList(T, NOM, ...) constexpr std::array<T, ::countIL({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

defineLiteralList(int, foo, 1, 2, 5);

template<LiteralList<int> L> struct bar {
  template<size_t i> static void print() {
    constexpr size_t len = L.data[i];
    int data[len];
    for(size_t j = 0;j < len;j++)
      data[j] = L.data[j%L.len];
    for(size_t j = 0;j < len;j++)
      std::cout << data[j] << std::endl;
  };
};

int main(int argc, char** argv) {
  bar<foo>::print<2>();
}


