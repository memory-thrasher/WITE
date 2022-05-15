#include "DBEntity.hpp"

namespave WITE::DB {

  void DBEntity::read(DBRecord* dst) {
    db->read(this, dst);
  }

  void DBEntity::read(void* dst, size_t maxSize, size_t offset) {
    DBRecord record;
    read(&record);
    if(offset > DBRecord::CONTENT_SIZE) {
      db->getEntity(record.header.nextGlobalId)->read(dst, maxSize, offset - DBRecord::CONTENT_SIZE);
    } else {
      size_t overlap = min(DBRecord::CONTENT_SIZE, maxSize) - offset;
      memcpy(dst, reinterpret_cast<void*>(&record.content + offset), overlap);
      if(maxSize > DBRecord::CONTENT_SIZE - offset) {
	assert_warn(record.header.flags & DBRecord::flag_t::has_next, "DB large object read underflow");
	db->getEntity(record.nextGlobalId)->read(dst + overlap, maxSize - overlap, 0);
      }
    }
  }

  void DBEntity::write(DBDelta* delta) {
    delta->targetEntityId = idx;
    db->write(delta);
  }

  void DBEntity::write(void* src, size_t len, size_t offset) {
    DBDelta delta;
    delta.clear();
    while(offset < DBRecord::CONTENT_SIZE && len) {
      delta.len = glm::min(DBDelta::MAX_DELTA_SIZE, DBRecord::CONTENT_SIZE - offset - len);
      memcpy(reinterpret_cast<void*>(delta.content), src, delta.len);
      src += delta.len;
      offset += delta.len;
      len -= delta.len;
      write(&delta);
    }
    if(len + offset > DBRecord::CONTENT_SIZE) {
      db->getEntityAfter(this)->write(src, len, offset);
    }
  }

  void DBEntity::writeFlags(DBRecord::flag_t mask, DBRecord::flag_t values) {
    DBDelta delta;
    delta.clear();
    delta.flagWriteMask = mask;
    delta.flagWriteValues = values;
    write(&delta);
  }

  void DBEntity::setType(DBRecord::type_t type) {
    DBDelta delta;
    delta.clear();
    delta.write_type = 1;
    delta.new_type = type;
    write(&delta);
  }

  bool DBEntity::isUpdatable() {
    DBRecord record;
    read(&record);
    return isUpdatable(&record);
  }

  /*static*/ bool DBEntity::isUpdatable(DBRecord* r) {
    return (r.header.flags & DBRecord::head_node) && db->getType(r.header.type)->update != NULL;
  }

}
