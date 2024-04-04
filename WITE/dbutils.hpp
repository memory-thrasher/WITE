#pragma once

#include "dbTemplateStructs.hpp"

namespace WITE {

  template<literalList<dbColumn> CS, size_t OFFSET = 0, size_t LOG_OFFSET = 0> struct columnAttributes {
    static constexpr size_t offset = OFFSET, logOffset = LOG_OFFSET;
    static constexpr dbColumn C = CS[0];
    static constexpr bool hasLog = C.sync != dbSyncMethod::immediate;
    columnAttributes<CS.sub(1), offset + C.size, logOffset + (hasLog ? C.size : 0)> rest;

    template<uint64_t id> consteval auto find() {
      if constexpr(id == C.id)
	return *this;
      else
	return rest.find<id>();
    };

    template<uint64_t idx> consteval auto at() {
      if constexpr(idx == 0)
	return *this;
      else
	return rest.at<idx-1>();
    };

    consteval auto end() {
      if constexpr(CS.len > 1)
	return rest.end();
      else
	return rest;
    };
  };

  template<size_t OFFSET, size_t LOG_OFFSET> struct columnAttributes<literalList<dbColumn> {}, OFFSET, LOG_OFFSET> {
    static constexpr size_t SIZE = OFFSET, LOG_SIZE = LOG_OFFSET;
  };

}
