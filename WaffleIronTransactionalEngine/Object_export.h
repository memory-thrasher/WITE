#pragma once

namespace WITE {

  class export_def Object {
  public:
    Object() = default;
    Object(const Object&) = delete;
    ~Object() = default;
    virtual Transform getTrans() = 0;
    virtual void pushTrans(Transform*) = 0;
    virtual void setName(const char*) = 0;
    virtual void setNameF(const char*, ...) = 0;
    virtual const char* getName() = 0;
    virtual Renderer* getRenderer(WITE::renderLayerIdx) = 0;
    static Object* make(Database::Entry start, size_t transformOffset, WITE::Database::Entry* map);
  };

}
