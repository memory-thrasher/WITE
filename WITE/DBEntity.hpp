/*
enttiy class contains metadata only, and is held internally by the db in ram to track the location and other info of a virtual object. Handles to entities are handed off to the worker thread for that and other threads' updates.
 */

namespace WITE::DB {

  class Database;
  class DBDelta;

  class DBEntity {
  private:
    size_t masterThread;//assigned on db load
    volatile DBDelta *firstLog, *lastLog;//these are write-protected by Database::logMutex
    size_t idx,;//location of the corrosponding record in the main db file
    Database* db;
    //TODO some kind of work for update tracking to help decide which entities get moved during thread load balancing
    friend class Database;
    DBEntity(const DBEntity&) = delete;
    DBEntity() = delete;
    DBEntity(Database* db, size_t idx) : db(db), idx(idx), masterThread(~0) {}
  public:
    static constexpr size_t DATA_SIZE = 4096;
    static constexpr size_t USER_DATA_SIZE = DATA_SIZE - sizeof(header_t);
    static constexpr flag_t FLAG_ALLOCATED = 1, FLAG_ISDATA = 1<<1;
    void read(class DBRecord* dst);
    void read(void* dst, size_t maxSize, size_t offset);//maxSize because the record might be bigger than the stored data
    template<typename T> inline void read(T* dst) { read(reinterpret_cast<void*>(dst), sizeof(T)); };
    template<typename T, typename V, V T::* M> inline void read(T* dst) {
      read(reinterpret_cast<void*>(dst), sizeof(M), WITE::Util::member_offset(T, M));
    };
    //TODO range / specific fields
    void write(class DBDelta* src);
    void write(void* src, size_t len, size_t offset = 0);
    template<typename T> inline void write(T* src) { write(reinterpret_cast<void*>(src), sizeof(T)); };
    template<typename T, typename V, V T::* M> inline void write(T* src) {
      write(reinterpret_cast<void*>(src), sizeof(M), WITE::Util::member_offset(T, M));
    };
    //TODO member pointer variadic
    //TODO shortcuts for flag-only writes
  }

}
