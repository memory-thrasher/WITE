/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "dbTable.hpp"
#include "dbIndexTuple.hpp"

namespace WITE {

  template<class... REST> struct dbTableTuple {//terminal case first

    dbTableTuple(const std::filesystem::path&, bool, bool) {};

    inline dbTableBase* get(uint64_t id) {
      return NULL;
    };

  };

  template<class R, class... REST> struct dbTableTuple<R, REST...> {

    dbTable<R> data;
    dbTableTuple<REST...> rest;
    dbIndexTupleFor<R> indices;

    dbTableTuple(const std::filesystem::path& basedir, bool clobberMaster, bool clobberLog) :
      data(basedir, R::dbFileId, clobberMaster, clobberLog),
      rest(basedir, clobberMaster, clobberLog),
      indices(basedir, clobberLog)
    {
      ASSERT_TRAP(clobberMaster || !clobberLog, "illegal argument: clobber master but keep log");
    };

    template<uint64_t ID> inline auto& get() {
      if constexpr(ID == R::typeId) {
	return data;
      } else {
	return rest.template get<ID>();
      }
    }

    inline dbTableBase* get(uint64_t id) {
      if(id == R::typeId)
	return &data;
      return rest.get(id);
    }

    template<uint64_t ID> inline auto& getIndices() {
      if constexpr(ID == R::typeId) {
	return indices;
      } else {
	return rest.template getIndices<ID>();
      }
    }

  };

}
