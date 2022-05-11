#include "DBThread.hpp"

namespace WITE::DB {

  void DBDelta::applyTo(DBRecord* out) {//may be in the data mmap, may be a temp during a read op
    if(len)
      memcpy(reinterpret_cast<void*>(&out->content[dstStart]), reinterpret_cast<void*>(content), len);
    out->header.flags ^= flagWriteMask & (out->header.flags ^ flagWriteValues);
    if(write_nextGlobalId)
      out->nextGlobalId = new_nextGlobalId;
    if(write_type)
      out->type = new_type;
  }

}
