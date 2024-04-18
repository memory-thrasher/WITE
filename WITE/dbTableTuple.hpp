#pragma once

#include "dbTable.hpp"

namespace WITE {

  template<class... REST> struct dbTableTuple {//terminal case first

    dbTableTuple(std::string&, bool, bool) {};

    inline dbTableBase* get(uint64_t id) {
      return NULL;
    };

  };

  template<class R, class... REST> struct dbTableTuple<R, REST...> {

    dbTable<R> data;
    dbTableTuple<REST...> rest;

    dbTableTuple(std::string& basedir, bool clobberMaster, bool clobberLog) :
      data(basedir, R::dbFileId, clobberMaster, clobberLog),
      rest(basedir, clobberMaster, clobberLog)
    {};

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

  };

}
