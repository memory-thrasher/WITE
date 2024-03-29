#pragma once

#include <ostream>

#include "DBRecord.hpp"
#include "stdExtensions.hpp"

namespace WITE::DB {

  class Database;

  class DBDelta {
  public:
    static constexpr size_t MAX_DELTA_SIZE = 16*4*2;//2*sizeof(glm::mat4x4);
    uint64_t frame;
    size_t dstStart, len;
    uint8_t content[MAX_DELTA_SIZE];
    size_t targetEntityId;// = DBEntity::globalId = index of Database::metadata
    size_t new_nextGlobalId;
    DBRecord::type_t new_type;
    bool write_nextGlobalId, write_type;
    DBDelta* nextForEntity;
    DBRecordFlag flagWriteMask, flagWriteValues;
    DBDelta() : frame(~0) {}//for constructing temps and arrays, so no need to init very much
    DBDelta(const DBDelta&);//copy constructor that only copies the first len bytes of content
    void applyTo(DBRecord*) const;
    void clear();
    DBDelta& operator=(const DBDelta& src) noexcept;
    void integrityCheck(Database* = NULL) const;
    bool operator==(const DBDelta& other) const {
      if(frame != other.frame || dstStart != other.dstStart || len != other.len || targetEntityId != other.targetEntityId ||
	 write_nextGlobalId != other.write_nextGlobalId || write_type != other.write_type ||
	 (flagWriteMask & flagWriteValues) != (other.flagWriteMask & other.flagWriteValues))
	return false;
      if(write_nextGlobalId && new_nextGlobalId != other.new_nextGlobalId)
	return false;
      if(write_type && new_type != other.new_type)
	return false;
      if(len && memcmp(content, other.content, len) != 0)
	return false;
      return true;
    };
    bool operator!=(const DBDelta& other) const { return !(*this == other); };
  };//size=190

  template<class T>
  std::basic_ostream<T>& operator<<(std::basic_ostream<T>& str, const DBDelta& db) {
    //for debugging
    str << "{ target: " << db.targetEntityId << ", frame: " << db.frame;
    if(db.len)
      str << ", content: { offset: " << db.dstStart << ", len: " << db.len << ", firstBit: " << ((uint64_t*)db.content)[0] << " }";
    if(db.write_nextGlobalId)
      str << ", new_nextGlobalId: " << db.new_nextGlobalId;
    if(db.write_type)
      str << ", new_type: " << db.new_type;
    if(db.flagWriteMask != 0)
      str << ", flags: { mask: " << (int)db.flagWriteMask << ", values: " << (int)db.flagWriteValues << " }";
    if(db.nextForEntity)
      str << ", nextForEntity: " << db.nextForEntity;
    return str;
  };

}
