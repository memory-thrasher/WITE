#pragma once

#include "dbTemplateStructs.hpp"
#include "dbUtils.hpp"

namespace WITE {

  template<literalList<dbTable DBT> struct database {

    std::atomic_uint64_t currentFrame = 1, flushingFrame = 0;

    template<dbTable T> struct table {

      static constexpr columnAttributes<T.columns> CAS {};

      //

    };

    template<literalList<dbTable> TS> struct tableTuple {

      static constexpr dbTable T = TS[0];
      table<T> data;
      tableTuple<TS.sub(1)> rest;

      template<uint64_t id> inline auto& get() {
	if constexpr(T.id == id)
	  return data;
	else
	  return rest.get<id>();//compiler error here if id does not exist in TS
      };

    };

    template<> struct tableTuple<literalList<dbTable>{}> : std::false_type {};

    tableTuple<DBT> tables;

  };

};
