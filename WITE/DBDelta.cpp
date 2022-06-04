#include "DBThread.hpp"

namespace WITE::DB {

  void DBDelta::applyTo(DBRecord* out) {//may be in the data mmap, may be a temp during a read op
    if(len)
      memcpy(&out->content[dstStart], content, len);
    out->header.flags ^= flagWriteMask & (out->header.flags ^ flagWriteValues);
    if(write_nextGlobalId)
      out->nextGlobalId = new_nextGlobalId;
    if(write_type)
      out->type = new_type;
  }

  void DBDelta::clear() {
    dstStart = len = 0;
    write_nextGlobalId = write_type = 0;
    nextForEntity = NULL;
    flagWriteMask = 0;
  }

}
