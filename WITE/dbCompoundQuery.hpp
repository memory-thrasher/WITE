/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "dbTable.hpp"

namespace WITE {

  //use a compound query if multiple objects must be read and/or written without any of them changing. Avoid when possible.
  //reads of a prior frame's data is preferred because it won't change and so does not need to be locked
  class dbCompoundQuery {
  private:
    std::set<syncLock*> mutexes;//row locks. Sorted by address so the locking order is consistent.
    std::vector<scopeLock> locks;
    //TODO implement if needed. Can't think of a good reason to do this, nor proof that it won't ever be needed.
  };
