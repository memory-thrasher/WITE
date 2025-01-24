/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <iostream>

struct Bar {
  int x, y;
};

template<Bar B> struct Foo {
  template<class L> static consteval int defer(L l) {
    return l(B);
  };
};

consteval int cef(int x, int y) {
  return x + y/2;
};

template<Bar B> struct barDataBundle {
  // static constexpr int filteredNumbersLength = Foo<B>::defer([](Bar b) consteval { return cef(6, b.x); });
  // int filteredNumbers[filteredNumbersLength];
  static consteval int filteredNumbers2Length() {
    return Foo<B>::defer([](Bar b) consteval { return cef(6, b.x); });
  };
  int filteredNumbers2[filteredNumbers2Length()];
};

int main(int argc, char** argv) {
  barDataBundle<Bar {4, 5}> bdb;
  std::cout << sizeof(bdb.filteredNumbers2) << std::endl;
};

