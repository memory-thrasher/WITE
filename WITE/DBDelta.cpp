#include "DBThread.hpp"
#include "Database.hpp"
#include "DEBUG.hpp"
#include "StdExtensions.hpp"

namespace WITE::DB {

  void DBDelta::applyTo(DBRecord* out) const {//may be in the data mmap, may be a temp during a read op
    ASSERT_TRAP(!len || dstStart < MAX_DELTA_SIZE, "dstStart out of bounds");
    if(len)
      memcpy(&out->content[dstStart], content, len);
    out->header.flags = out->header.flags ^ (flagWriteMask & (out->header.flags ^ flagWriteValues));
    if(write_nextGlobalId)
      out->header.nextGlobalId = new_nextGlobalId;
    if(write_type)
      out->header.type = new_type;
  }

  void DBDelta::clear() {
    dstStart = len = 0;
    write_nextGlobalId = write_type = 0;
    nextForEntity = NULL;
    flagWriteMask = DBRecordFlag::none;
  }

  DBDelta::DBDelta(const DBDelta& src) {
    *this = src;
  }

  void DBDelta::integrityCheck(Database* db) const {
    if(db) {
      ASSERT_TRAP(frame <= db->getFrame(), "corrupt delta! frame");
      ASSERT_TRAP(targetEntityId <= db->getEntityCount(), "corrupt delta! target");
      ASSERT_TRAP(!write_nextGlobalId || new_nextGlobalId == ~0 || new_nextGlobalId <= db->getEntityCount(), "corrupt delta! next id");
      //ASSERT_TRAP(nextForEntity == 0 || (nextForEntity >= &
      //ASSERT_TRAP(!write_type || TODO
    }
    ASSERT_TRAP(dstStart + len <= MAX_DELTA_SIZE, "corrupt delta! data overflow");
    ASSERT_TRAP(!write_nextGlobalId || new_nextGlobalId != targetEntityId, "corrupt delta! loop");
  }

  DBDelta& DBDelta::operator=(const DBDelta& src) noexcept {
    src.integrityCheck();
    frame = src.frame;
    dstStart = src.dstStart;
    len = src.len;
    memcpy(content, src.content, len);
    targetEntityId = src.targetEntityId;
    write_nextGlobalId = src.write_nextGlobalId;
    if(write_nextGlobalId)
      new_nextGlobalId = src.new_nextGlobalId;
    write_type = src.write_type;
    if(write_type)
      new_type = src.new_type;
    nextForEntity = NULL;//src.nextForEntity;//do not copy linked list pointers
    flagWriteMask = src.flagWriteMask;
    if(flagWriteMask != 0)
      flagWriteValues = src.flagWriteValues;
    this->integrityCheck();
    return *this;
  };

}
