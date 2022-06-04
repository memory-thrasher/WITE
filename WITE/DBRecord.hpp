#pragma once

namespace WITE::DB {

  typedef struct DBRecord {
  public:
    typedef enum flag_t : uint16_t {
       allocated = (1 << 0),
       head_node = (1 << 1),
       has_next  = (1 << 2),
    } flag_t;
    typedef uint64_t type_t;//user-defined type
    typedef struct header {
      size_t nextGlobalId;//linked list, for large objects only (uncommon/discouraged)
      type_t type;
      flag_t flags;
    } header_t;
    static constexpr size_t CONTENT_SIZE = 4096 - sizeof(header_t);
    header_t header;
    uint8_t content[CONTENT_SIZE];
  } DBRecord;

}
