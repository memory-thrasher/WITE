#pragma once

#include <stdlib.h>

#include "literalList.hpp"

namespace WITE {

  enum class dbSyncMethod {
    lock, //mutex used internally for read and write, current data always available, logs contain copies of the new data
    worm, //write one read many, only one update allowed per frame, only previous frame data may be read, logs are copies
    atomic, //logs contain delta operations like increment and clamp, data is as current as the read call, older data available
    immediate, //no logs or synchronization, caller must prevent concurrent access. Use with care.
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
