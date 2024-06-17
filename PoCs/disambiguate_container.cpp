/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <ranges>
#include <vector>
#include <iostream>

template<std::ranges::contiguous_range R> requires std::convertible_to<std::ranges::range_reference_t<R>, int> 
void foo(R& r) {
  std::cout << "contiguous of first element: " << r[0] << std::endl;
};

template<std::ranges::range R> requires std::convertible_to<std::ranges::range_reference_t<R>, int> 
  void foo(R& r) {
  std::cout << "noncontiguous of first elemet: " << *r.begin() << std::endl;
};

void foo(std::initializer_list<int> r) {
  foo<std::initializer_list<int>>(r);
};

int main()
{
    int x[4] {1};
    foo(x);
    std::vector<int> y;
    y.push_back(7);
    foo(y);
    foo({ 12 });
}
