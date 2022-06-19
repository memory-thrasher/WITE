#pragma once

#include "DBRecord.hpp"
#include "DBEntity.hpp"

namespace WITE::DB {

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
    void applyTo(DBRecord*);
    void clear();
    DBDelta& operator=(const DBDelta& src) noexcept {
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
      return *this;
    };
  };//size=190

}
