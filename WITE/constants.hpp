/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

//if you need an include, it doesn't belong here
//defines wrapped in ifndef mean they can be overriden at compile-time i.e. -D. Both the shared lib and any application must have the same override(s)!

#ifndef MAX_GPUS
#define MAX_GPUS 8
#endif

#ifndef MIN_LOG_HISTORY
#define MIN_LOG_HISTORY 3
#endif

#define CONCAT1(a, b) a ## b
#define CONCAT(a, b) CONCAT1(a, b)
#define UNIQUENAME(prefix) CONCAT(prefix, __LINE__)

namespace WITE {

};
