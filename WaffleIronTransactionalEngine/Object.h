#pragma once

#include "Export.h"
#include "Transform.h"
#include "Renderer.h"

class Object
{
public:
  Object(WITE::Database::Entry start, size_t transformOffset) : start(start), activeTransform(start, transformOffset) {};
  ~Object();
  Transform getTrans();//frame batched internally by Transform
  void pushTrans();//updates location record in db entry
protected:
  void* objData;//??
private:
  template<typename T = void> class Subresource {//TODO make this export?
  public:
    Subresource(WITE::Database::Entry start, size_t offsetFromStart, size_t size) {//see from WITE::Database read, but recursive
      size_t dataSectionSize = WITE::Database::BLOCKDATASIZE - offsetFromStart % WITE::Database::BLOCKDATASIZE;
      parent = getChildEntryByIdx(start, offsetFromStart / WITE::Database::BLOCKDATASIZE);
      next = (size > dataSectionSize) ? std::make_unique(start, offsetFromStart + dataSectionSize, size - dataSectionSize) :
	std::nullptr;
    };
    template<class U = T, typename = std::enable_if_t<!std::is_same<U, void>::value>>
    Subresource(WITE::Database::Entry start, size_t offsetFromStart) : this(start, offsetFromStart, sizeof(U)) {};
    //size_t is the size of an int that can index memory, so is assumed to be the size of a register.
    template<class U = T, typename = std::enable_if_t<!std::is_same<U, void>::value && sizeof(U) <= sizeof(size_t)>>
    U get() {//for primitives, otherwise use output reference
      U ret;
      get(&ret);
      return ret;
    };
    template<class U = T> std::enable_if_t<!std::is_same<U, void>::value> get(U* out) {
      get(static_cast<uint8_t*>(out));
    };
    void get(uint8_t* out) {
      database->get(parent, out, offsetIntoEntry, size);
      if (next) next->get(out + size);
    };
    size_t offsetFromStart, offsetIntoEntry, size, entries;//size is only the portion within this entry
    WITE::Database::Entry parent;//object may have multiple entries. Track which contains this subresource to save reads of list nodes.
    std::unique_ptr<class Subresource<void>> next;
  };
  WITE::Database::Entry start;
  class Subresource<glm::dmat4x4> activeTransform;
  class Renderer renderLayer[MAX_RENDER_LAYERS];//change only via Renderer::bind; transient
  friend class Renderer;
};

