/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <sys/types.h>
#include <stdint.h>
#include <array>

struct rgb32 {
  float r, g, b;
};

template<size_t L> struct pixelArray {
  std::array<rgb32, L> data;
  static_assert(sizeof(data) == sizeof(uint32_t) * 3 * L);
};

int main(int argc, const char** argv) {
  pixelArray<19> data;
  data.data[0].r = 7;
  return data.data[0].r;
}

