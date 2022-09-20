/*
enttiy class contains metadata only, and is held internally by the db in ram to track the location and other info of a virtual object. Handles to entities are handed off to the worker thread for that and other threads' updates.
 */

#pragma once

#include "Callback.hpp"
#include "Utils_Memory.hpp"
#include "DBRecord.hpp"
#include "RollingQueue.hpp"
#include "DBDelta.hpp"
#include "AtomicLinkedList.hpp"

namespace WITE::DB {

  class Database;

  class DBEntity {
  private:
    uint64_t lastWrittenFrame = 0;
    size_t masterThread;
    Collections::AtomicLinkedList<DBDelta, &DBDelta::nextForEntity> log;
    size_t idx;//location of the corrosponding record in the main db file
    Database* db;
    DBEntity* nextOfTypeInThread;//protected by owning thread
    //TODO some kind of work for update tracking to help decide which entities get moved during thread load balancing
    friend class Database;
    friend class DBThread;
    DBEntity(const DBEntity&) = delete;
    DBEntity() = delete;
    DBEntity(Database* db, size_t idx) : masterThread(~0), idx(idx), db(db), nextOfTypeInThread(NULL) {}
  public:
    auto inline getId() { return idx; };
    void read(DBRecord* dst);
    void read(uint8_t* dst, size_t maxSize, size_t offset = 0);//maxSize because the record might be bigger than the stored data
    template<typename T, typename RET = std::enable_if_t<!std::is_same_v<uint8_t, T>, void>>
    RET read(T* dst, size_t len, size_t offset = 0) { read(reinterpret_cast<uint8_t*>(dst), len * sizeof(T), offset); };
    template<typename T> inline void read(T* dst) { read(reinterpret_cast<uint8_t*>(dst), sizeof(T)); };
    template<typename T, typename V, V T::* M> inline void read(T* dst) {
      read(reinterpret_cast<uint8_t*>(dst), sizeof(V), WITE::Util::member_offset(M));
    };
    void completeRead(uint8_t* out, DBRecord* record, size_t len);
    template<typename T> void completeRead(T* out, DBRecord* record) {
      completeRead(reinterpret_cast<uint8_t*>(out), record, sizeof(T));
    };
    //TODO member pointer variadic
    void write(DBDelta* src);
    void write(const uint8_t* src, size_t len, size_t offset = 0);
    template<typename T, typename RET = std::enable_if_t<!std::is_same_v<uint8_t, T>, void>>
    RET write(const T* src, size_t len, size_t offset = 0) {
      write(reinterpret_cast<const uint8_t*>(src), len * sizeof(T), offset);
    };
    template<typename T> inline void write(const T* src) { write(reinterpret_cast<const uint8_t*>(src), sizeof(T)); };
    template<typename T, typename V, V T::* M> inline void write(T* src) {
      write(reinterpret_cast<const uint8_t*>(src), sizeof(M), WITE::Util::member_offset(M));
    };
    //TODO member pointer variadic
    void writeFlags(DBRecordFlag mask, DBRecordFlag values);
    DBRecord::type_t getType();
    void setType(DBRecord::type_t type);
    bool isUpdatable();
    static bool isUpdatable(DBRecord::header_t* h, Database* db);
    static bool isUpdatable(bool isHead, DBRecord::type_t type, Database* db);
    void destroy(DBRecord* data = NULL);
    Database* getDb() { return db; };
  };

}
