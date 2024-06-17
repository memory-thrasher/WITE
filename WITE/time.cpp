/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "time.hpp"

namespace WITE {

  constexpr int64_t NS_PER_SEC = 1000000000;

  timespec now() {
    timespec ret;
    clock_gettime(CLOCK_MONOTONIC, &ret);
    return ret;
  };

  timespec since(const timespec& b) {
    auto n = now();
    return diff(n, b);
  };

  timespec diff(const timespec& a, const timespec& b) {
    if(a.tv_nsec < b.tv_nsec)
      return {
	a.tv_sec - b.tv_sec - 1,
	NS_PER_SEC + a.tv_nsec - b.tv_nsec
      };
    else
      return {
	a.tv_sec - b.tv_sec,
	a.tv_nsec - b.tv_nsec
      };
  };

  uint64_t toNS(const timespec& t) {
    return NS_PER_SEC * t.tv_sec + t.tv_nsec;
  };

}
