#pragma once

#include "dbtable.hpp"

namespace WITE {

  template<class R, class... REST> struct dbtableTuple {

    dbtable<R> data;
    dbtableTuple<REST...> rest;

    dbtableTuple(std::string& basedir, bool clobber) :
      data(basedir, R::dbFileId, clobber),
      rest(basedir, clobber)
    {};

    template<uint64_t ID> inline auto& get() {
      if constexpr(ID == R::typeId) {
	return data;
      } else {
	return rest.get<ID>();
      }
    }

    inline dbtableBase* get(uint64_t id) {
      if(id == R::typeId)
	return &data;
      return rest.get(id);
    }

  };

  template<> struct dbtableTuple<> : std::false_type {

    dbtableTuple(std::string&, bool) {};

    inline dbtableBase* get(uint64_t id) {
      return NULL;
    };

  };

}
