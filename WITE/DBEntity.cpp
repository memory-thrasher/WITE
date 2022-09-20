#include "DBEntity.hpp"

#include "Database.hpp"
#include "DEBUG.hpp"

namespace WITE::DB {

  void DBEntity::destroy(DBRecord* data) {
    db->deallocate(this, data ? &data->header : NULL);
  }

  void DBEntity::read(DBRecord* dst) {
    db->read(this, dst);
  }

  void DBEntity::read(uint8_t* dst, size_t maxSize, size_t offset) {
    DBRecord record;
    if(offset > DBRecord::CONTENT_SIZE) {
      db->readHeader(this, &record.header, true, false, false);
      db->getEntity(record.header.nextGlobalId)->read(dst, maxSize, offset - DBRecord::CONTENT_SIZE);
    } else {
      read(&record);
      size_t overlap = Util::min(DBRecord::CONTENT_SIZE, maxSize) - offset;
      memcpy(dst, &record.content + offset, overlap);//TODO don't memcpy content twice, content-only read into dst
      if(maxSize > DBRecord::CONTENT_SIZE - offset) {
    	ASSERT_TRAP((record.header.flags & DBRecordFlag::has_next) != 0, "DB large object read underflow");
    	db->getEntity(record.header.nextGlobalId)->read(dst + overlap, maxSize - overlap, 0);
      }
    }
  }

  void DBEntity::write(DBDelta* delta) {
    delta->targetEntityId = idx;
    db->write(delta);
  }

  void DBEntity::write(const uint8_t* src, size_t len, size_t offset) {
    DBDelta delta;
    delta.clear();
    while(offset < DBRecord::CONTENT_SIZE && len) {
      delta.len = Util::min(DBDelta::MAX_DELTA_SIZE, DBRecord::CONTENT_SIZE - offset, len);
      memcpy(delta.content, src, delta.len);
      src += delta.len;
      offset += delta.len;
      len -= delta.len;
      write(&delta);
    }
    if(len + offset > DBRecord::CONTENT_SIZE) {
      db->getEntityAfter(this)->write(src, len, offset);
    }
  }

  void DBEntity::writeFlags(DBRecordFlag mask, DBRecordFlag values) {
    DBDelta delta;
    delta.clear();
    delta.flagWriteMask = mask;
    delta.flagWriteValues = values;
    write(&delta);
  }

  DBRecord::type_t DBEntity::getType() {
    DBRecord::header_t header;
    db->readHeader(this, &header, false, true, false);
    return header.type;
  }

  void DBEntity::setType(DBRecord::type_t type) {
    DBDelta delta;
    delta.clear();
    delta.write_type = 1;
    delta.new_type = type;
    write(&delta);
  }

  bool DBEntity::isUpdatable() {
    DBRecord::header_t header;
    db->readHeader(this, &header);
    return isUpdatable(&header, db);
  }

  /*static*/ bool DBEntity::isUpdatable(DBRecord::header_t* h, Database* db) {
    return isUpdatable((h->flags & DBRecordFlag::head_node) != 0, h->type, db);
  }

  /*static*/ bool DBEntity::isUpdatable(bool isHead, DBRecord::type_t type, Database* db) {
    return isHead && db->getType(type)->onUpdate;
  }

  void DBEntity::completeRead(uint8_t* out, DBRecord* record, size_t len) {
    size_t thisLen = Util::min(len, DBRecord::CONTENT_SIZE);
    memcpy(out, record->content, thisLen);
    if(len > thisLen && (record->header.flags & DBRecordFlag::has_next) != 0) {
      db->getEntity(record->header.nextGlobalId)->read(out + DBRecord::CONTENT_SIZE, len - thisLen);
    }
  }

}
