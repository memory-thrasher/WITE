#pragma once
/*
namespace WITE::DFOSLG {

  constexpr uint64_t
    //transform flags
    TRANS_CLIP = 1, TRANS_PROJ = 2, TRANS_VIEW = 4, TRANS_OBJ = 8, TRANS_MVP = 0x10,
    TRANS_CLIP_INV = 0x100, TRANS_PROJ_INV = 0x200, TRANS_VIEW_INV = 0x400, TRANS_OBJ_INV = 0x800, TRANS_MVP_INV = 0x1000,
    //???
    PER_OBJECT = 1, PER_CAMERA = 2, PER_WINDOW = 4, PER_FRAME = 8, PER_CUBEFACE = 0x10, PER_LIGHT = 0x100;
  //TODO image flags?

  //TODO resource type: static (texture), instance (model transform), render (mvp transform), camera (depth stencil)
  //TODO identify which resources get used in each verb

  template<class staticInit, class execution> class Flow {//this is basically erverything one camera does during rendering //TODO
  public:
    void render(WITE::Queue::ExecutionPlan);
  };

  template<size_t id> class resource {
  public:
    static constexpr size_t ID = id;
  };

  class resourceConsumer {};//TODO; basically everything below should inhjerit for this, and it should have the pathway for finding usages for an image

  typedef std::tuple<> None;

  template<uint64_t contents, size_t id> class Transform : public resource<id> {};

  template<size_t id> class Image : public resource<id> {};

  template<size_t id> class DepthStencil : public resource<id> {};

  class Executable {
  public:
    //template<class Flow> Executable() = 0;//template virtual is not a thing, so can't enforce, but all children should have this constructor.
    virtual void operator()() const = 0;
  };

  template<class First, class... S> class SerialExecution : public Executable {
  private:
    First f;
    SerialExecution<S...> rest;
  public:
    template<class Flow> SerialExecution() : f<Flow>(), rest<Flow>() {};
    inline void operator()() const {
      f();
      rest();
    }
  };
  template<class Only> class SerialExecution<Only> : public Executable {
  private:
    Only o;
  public:
    template<class Flow> SerialExecution() : o<Flow>() {};
    inline void operator()() const {
      o();
    }
  };

  template<class ins, class outs> class Shader {};//TODO

  //class objectFilter {}; //bool operator()(WITE::Database::type, Entry) const//TODO
  template<class resources, class... Executable> class PerObject : public Executable {};//TODO//also TODO: can be used in resource declaration

  template<class T> class Load : public Executable {};//TODO

  template<class T> class Clear : public Executable {};//TODO //also can be an attachment (is that a thing?)

  template<class ins, class outs, class... shaders> class RenderPass : public Executable {};//TODO

  template<class T> class Present : public Executable {};//TODO

//quick and dirty way to generate unique id for resources, assuming you declare them all in the same file on different lines. If you have issues because you're declaring resources in different files, just manually assign a unique constexpr id per resource using typedef
#define declareDFOSLGImage(name, ...) typedef Image<__LINE__, ##__VA_ARGS__> name;

#include "DFOSLG_definitions_export.cpp"

}
*/
