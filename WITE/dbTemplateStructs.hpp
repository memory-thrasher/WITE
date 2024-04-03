#pragma once

#include <stdlib.h>

#include "literalList.hpp"

namespace WITE {

  enum class dbSyncMethod {
    lock, //mutex needed to read or write, current data always available
    worm, //write one read many, only one update allowed per frame, only previous frame data may be read
    atomic, //logs contain delta operations like increment and clamp, data is as current as the read call
  };

  struct dbColumn {
    uint64_t id;//only need by unique within the containing table
    dbSyncMethod sync;
    uint32_t size;
  };

  struct dbTable {
    uint64_t id;
    literalList<dbColumn> columns;
  };

}
