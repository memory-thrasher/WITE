/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

namespace WITE {

  class bitmaskIterator {
  private:
    size_t i;
    const int64_t value;
  public:
    bitmaskIterator(int64_t value) : i(0), value(value) {
      while((value & (1 << i)) == 0 && (value >> i) && i < sizeof(value) * 8) i++;
    };
    template<class T> bitmaskIterator(T value) : bitmaskIterator(static_cast<int64_t>(value)) {};
    bitmaskIterator(const bitmaskIterator& o) : i(o.i), value(o.value) {};
    bitmaskIterator() : bitmaskIterator(0) {};
    inline operator bool() const { return value >> i && i < sizeof(value) * 8; };//strange behavior: (1 >> 64) = 1
    inline const size_t& operator*() const { return i; };
    template<typename T> inline T asMask() const { return static_cast<T>(1 << i); };
    inline bitmaskIterator operator++() {
      i++;
      while((value & (1 << i)) == 0 && i < sizeof(value) * 8) i++;
      return *this;
    };
    inline bitmaskIterator operator++(int) {
      auto old = *this;
      operator++();
      return old;
    };
    inline bool operator==(const bitmaskIterator& o) const { return (!*this && !o) || (i == o.i && value == o.value); };
    inline bitmaskIterator begin() const { return *this; };
    inline bitmaskIterator end() const { return 0; };
  };

}
