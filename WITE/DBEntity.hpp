/*
enttiy class contains metadata only, and is held internally by the db in ram to track the location and other info of a virtual object. Handles to entities are handed off to the worker thread for that and other threads' updates.
 */

#pragma once

#include "Callback.hpp"
#include "Utils_Memory.hpp"
#include "DBRecord.hpp"
#include "RollingQueue.hpp"

namespace WITE::DB {

  class Database;
  class DBDelta;

  class DBEntity {
  private:
    size_t masterThread;//assigned on db load
    DBDelta * volatile firstLog, * volatile lastLog;//these are write-protected by Database::logMutex
    size_t idx;//location of the corrosponding record in the main db file
    Database* db;
    //TODO some kind of work for update tracking to help decide which entities get moved during thread load balancing
    friend class Database;
    DBEntity(const DBEntity&) = delete;
    DBEntity() = delete;
    DBEntity(Database* db, size_t idx) : masterThread(~0), idx(idx), db(db) {}
  public:
    void read(DBRecord* dst);
    void read(uint8_t* dst, size_t maxSize, size_t offset = 0);//maxSize because the record might be bigger than the stored data
    template<typename T> inline void read(T* dst) { read(reinterpret_cast<uint8_t*>(dst), sizeof(T)); };
    template<typename T, typename V, V T::* M> inline void read(T* dst) {
      read(reinterpret_cast<uint8_t*>(dst), sizeof(M), WITE::Util::member_offset(M));
    };
    //TODO member pointer variadic
    void write(DBDelta* src);
    void write(uint8_t* src, size_t len, size_t offset = 0);
    template<typename T> inline void write(T* src) { write(reinterpret_cast<uint8_t*>(src), sizeof(T)); };
    template<typename T, typename V, V T::* M> inline void write(T* src) {
      write(reinterpret_cast<uint8_t*>(src), sizeof(M), WITE::Util::member_offset(M));
    };
    //TODO member pointer variadic
    void writeFlags(DBRecordFlag mask, DBRecordFlag values);
    void setType(DBRecord::type_t type);
    bool isUpdatable();
    static bool isUpdatable(DBRecord* r, Database* db);
  };

  struct entity_type {
    DBRecord::type_t typeId;
    typedefCB(updateFn, void, DBRecord*);
    updateFn update;
  };

}
