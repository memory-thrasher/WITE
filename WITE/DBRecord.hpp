#pragma once

#include "DBRecordFlag.hpp"

namespace WITE::DB {

  typedef struct DBRecord {
  public:
    typedef uint64_t type_t;//user-defined type
    typedef struct header {
      size_t nextGlobalId;//linked list, for large objects only (uncommon/discouraged)
      type_t type;
      DBRecordFlag flags;
    } header_t;
    static constexpr size_t CONTENT_SIZE = 4096 - sizeof(header_t);
    header_t header;
    uint8_t content[CONTENT_SIZE];
  } DBRecord;

}
