/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <string.h> //for strcpy_s for windows

#include "stdExtensions.hpp"
#include "DEBUG.hpp"

namespace WITE {

  std::string concat(const std::initializer_list<const std::string> strings) {
    std::string ret;
    size_t totalLen = 0;
    for(const std::string& s : strings)
      totalLen += s.size();
    ret.reserve(totalLen);
    for(const std::string& s : strings)
      ret.append(s);
    return ret;
  }

  void strcpy(char* dst, const char* src, size_t dstLen) {
#ifdef iswindows //apparently windows thinks it can "depricate" part of the c++ standard, so this avoids a warning
    strcpy_s(dst, dstLen, src);
#else
    std::strcpy(dst, src);
#endif
  }

  void vsprintf(char* dst, size_t dstLen, const char* src, va_list vl) {
#ifdef iswindows //apparently windows thinks it can "depricate" part of the c++ standard, so this avoids a warning
    vsprintf_s(dst, dstLen, src, vl);
#else
    std::vsprintf(dst, src, vl);
#endif
  }

}
