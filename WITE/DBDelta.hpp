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
    DBEntity::flag_t flagWriteMask, flagWriteValues;
    DBDelta() : frame(~0);//for constructing temps and arrays, so no need to init very much
    DBDelta(const DBDelta&);//copy constructor that only copies the first len bytes of content
    void applyTo(class DBRecord*);
    void clear();
  };//size=190

}
