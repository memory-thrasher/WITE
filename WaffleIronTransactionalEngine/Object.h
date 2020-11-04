#pragma once

class Object : public WITE::Object
{
public:
  Object(WITE::Database::Entry start, size_t transformOffset, WITE::Database::Entry* map);
  ~Object();
  WITE::Transform getTrans();//frame batched internally by Transform
  void pushTrans(WITE::Transform*);//updates location record in db entry
  void setName(const char*);
  void setNameF(const char*, ...);
  const char* getName();
  bool isInitialized();
  WITE::Renderer* getRenderer(WITE::renderLayerIdx);
protected:
  void* objData;//??
private:
  std::atomic<uint64_t> creationFrame = std::numeric_limits<uint64_t>::max();
  char* name = 0;
  template<typename T = void> class Subresource {//TODO make this export?
  public:
    Subresource(WITE::Database::Entry start, size_t offsetFromStart, size_t size, WITE::Database::Entry* map) {//see WITE::Database read, but recursive
      offsetIntoEntry = offsetFromStart % WITE::Database::BLOCKDATASIZE;
      size_t dataSectionSize = WITE::Database::BLOCKDATASIZE - offsetIntoEntry;
      parent = map[offsetFromStart / WITE::Database::BLOCKDATASIZE];
      next = (size > dataSectionSize) ? std::make_unique<Subresource<void>>(start, offsetFromStart + dataSectionSize, size - dataSectionSize, map) : nullptr;
      this->size = WITE::min(size, dataSectionSize);
    };
    template<class U = T, typename = std::enable_if_t<!std::is_same<U, void>::value>>
    Subresource(WITE::Database::Entry start, size_t offsetFromStart, WITE::Database::Entry* map) :
      Subresource(start, offsetFromStart, sizeof(U), map) {};
    //size_t is the size of an int that can index memory, so is assumed to be the size of a register.
    template<class U = T, typename = std::enable_if_t<!std::is_same<U, void>::value && sizeof(U) <= sizeof(size_t)>>
    U get() {//for primitives, otherwise use output reference
      U ret;
      get(&ret);
      return ret;
    };
    template<class U = T> std::enable_if_t<!std::is_same<U, void>::value> get(U* out) {
      get(reinterpret_cast<uint8_t*>(out));
    };
    void get(uint8_t* out) {
      database->get(parent, out, offsetIntoEntry, size);
      if (next) next->get(out + size);
    };
    void put(uint8_t* in) {
      database->put(parent, in, offsetIntoEntry, size);
      if (next) next->put(in + size);
    }
    template<class U = T> std::enable_if_t<!std::is_same<U, void>::value> put(U* out) {
      put(reinterpret_cast<uint8_t*>(out));
    };
    size_t offsetIntoEntry, size;//size is only the portion within this entry
    WITE::Database::Entry parent;//object may have multiple entries. Track which contains this subresource to save reads of list nodes.
    std::unique_ptr<class Subresource<void>> next;
  };
  WITE::Database::Entry start;
  class Subresource<glm::dmat4x4> activeTransform;
  ::Renderer renderLayer[MAX_RENDER_LAYERS];//change only via Renderer::bind; transient
  WITE::SyncLock lock;//for Renderer
  friend class ::Renderer;
};

