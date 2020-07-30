#pragma once

#include <atomic>
#include <stdint.h>
#include <limits>
#include <vector>
#include <string>
#include <map>
#include <type_traits>
#include <iterator>
#include <memory>
#include "constants.h"
#include "WMath.h"

namespace WITE {

  typedef uint8_t renderLayerIdx;
  typedef uint64_t renderLayerMask;
  
  template<class RET, class... RArgs> class Callback_t {
  public:
    virtual RET call(RArgs... rargs) = 0;
    virtual ~Callback_t() = default;
  };

  template<class RET, class... RArgs> class CallbackFactory {
  private:
    template<class T, class... CArgs> class Callback : public Callback_t<RET, RArgs...> {
    private:
      RET(T::*x)(CArgs..., RArgs...);
      T * owner;
      std::tuple<CArgs...> cargs;
      RET call(RArgs... rargs) {
	return (*owner.*(x))(std::get<CArgs>(cargs)..., rargs...);
      };
    public:
      Callback(T* t, RET(T::*x)(CArgs..., RArgs...), CArgs... pda);
      ~Callback() {};
    };
    template<class... CArgs> class StaticCallback : public Callback_t<RET, RArgs...> {
    private:
      RET(*x)(CArgs..., RArgs...);
      std::tuple<CArgs...> cargs;
      RET call(RArgs... rargs) {
	return (*x)(std::get<CArgs>(cargs)..., rargs...);
      };
    public:
      StaticCallback(RET(*x)(CArgs..., RArgs...), CArgs... pda);
      ~StaticCallback() {};
    };
  public:
    typedef Callback_t<RET, RArgs...>* callback_t;
    template<class U, class... CArgs> static callback_t make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> static callback_t make(CArgs... cargs, RET(*func)(CArgs..., RArgs...));//for non-members or static members
  };
  template<class RET2, class... RArgs2> template<class T2, class... CArgs2>
  CallbackFactory<RET2, RArgs2...>::Callback<T2, CArgs2...>::Callback(T2* t, RET2(T2::*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), owner(t), cargs(std::forward<CArgs2>(pda)...) {}

  template<class RET2, class... RArgs2> template<class... CArgs2>
  CallbackFactory<RET2, RArgs2...>::StaticCallback<CArgs2...>::StaticCallback(RET2(*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), cargs(std::forward<CArgs2>(pda)...) {}

  template<class RET, class... RArgs> template<class U, class... CArgs> Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };
  
#define typedefCB(name, ...) typedef WITE::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ;

  typedefCB(rawDataSource, int, void*, size_t)

  class export_def ShaderResource {
  public:
  virtual void load(rawDataSource) = 0;
  virtual void* map() = 0;
  virtual void unmap() = 0;
  virtual size_t getSize() = 0;
  //virtual BackedBuffer* getBuffer() = 0;
  virtual ~ShaderResource() = default;
  };

  class export_def Shader {//not shared ptrs because these should be global in the game somewhere.
  public:
  struct resourceLayoutEntry {
    size_t type, stage, perInstance;
    void* moreData;
  };
  static Shader* make(const char* filepathWildcard, struct WITE::Shader::resourceLayoutEntry*, size_t resources);//static collection contains all
  static Shader* make(const char** filepath, size_t files, struct WITE::Shader::resourceLayoutEntry*, size_t resources);
  virtual ~Shader() = default;
  };

  typedef union Vertex {
    float floats[0];
    struct {
      glm::vec3 pos, data;
    };
    struct {
      float x, y, z, r, g, b;
    };
    Vertex(float x, float y, float z, float r, float g, float b) : pos(x, y, z), data(r, g, b) {}
    Vertex() : pos(), data() {}
    inline operator void*() {//for memcpy
      return static_cast<void*>(this);
    };
    inline float& operator[](size_t idx) {
      return floats[idx];
    };
    inline const float& operator[](size_t idx) const {
      return floats[idx];
    };
  } Vertex;

  class export_def MeshSource {//mesh source can redirect to file load or cpu procedure, but should not store any mesh data. Mesh does that, per LOD.
  public:
  virtual uint32_t populateMeshCPU(Vertex*, uint64_t maxVerts, const glm::dvec3* viewOrigin) = 0;//returns number of verts used
  virtual Shader* getComputeMesh(void) { return NULL; };
  virtual BBox3D* getBbox(BBox3D* out = NULL) = 0;//not transformed
  virtual ~MeshSource() = default;
  };

  class export_def StaticMesh : public MeshSource {//for debug or simple things only, don't waste host ram on large ones
  public:
    StaticMesh(const Vertex* data, uint32_t size) : data(data), size(size),
      box(mangle<Mangle_ComponentwiseMin<6, Vertex>, Vertex>(data, size).pos,
      mangle<Mangle_ComponentwiseMax<6, Vertex>, Vertex>(data, size).pos) {}
    uint32_t populateMeshCPU(Vertex* out, uint64_t maxVerts, const glm::dvec3* viewOrigin) {
      if (size <= maxVerts) {
	memcpy(out, data, size * FLOAT_BYTES);
	return size;
      }
      return 0;
    };
    BBox3D* getBbox(BBox3D* out) { return &box; };
    //return newOrHere(out)BBox3D(box); };
  private:
    BBox3D box;
    const void* data;
    uint32_t size;
  };

  class export_def Mesh {
  public:
    static std::shared_ptr<Mesh> make(MeshSource* source);
    Mesh(const Mesh&) = delete;
    virtual ~Mesh() = default;
  protected:
    Mesh() = default;
  };

  class Object;//defined in Database.h

  class export_def Renderer {
  public:
    static void bind(Object* o, Shader* s, std::shared_ptr<Mesh> m, WITE::renderLayerIdx rlIdx);
    static void unbind(Object*, WITE::renderLayerIdx);
  };

  class Window;
  
  class export_def Camera {
  public:
  Camera(const Camera&) = delete;
  static Camera* make(Window*, IntBox3D);//window owns camera object
  virtual ~Camera() = default;
  virtual void resize(IntBox3D) = 0;
  virtual float getPixelTangent() = 0;
  virtual float approxScreenArea(BBox3D*) = 0;
  virtual glm::dvec3 getLocation() = 0;
  virtual void setLocation(glm::dvec3) = 0;
  virtual void setMatrix(glm::dmat4&) = 0;
  virtual void setMatrix(glm::dmat4*) = 0;
  virtual Window* getWindow() = 0;
  virtual IntBox3D getScreenRect() = 0;
  virtual void setFov(double) = 0;
  virtual double getFov() = 0;
  virtual bool appliesOnLayer(renderLayerIdx i) = 0;
  virtual void setLayermaks(WITE::renderLayerMask newMask) = 0;
  protected:
  Camera() = default;
  };

  class export_def Window {
  public:
  Window(const Window&) = delete;//no copy
  virtual ~Window() = default;
  virtual size_t getCameraCount() = 0;
  virtual WITE::IntBox3D getBounds() = 0;
  virtual void setSize(uint32_t width, uint32_t height) = 0;
  virtual void setBounds(IntBox3D) = 0;
  virtual void setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
  virtual Camera* addCamera(IntBox3D) = 0;
  virtual Camera* getCamera(size_t idx) = 0;
  static std::unique_ptr<Window> make(size_t display = 0);
  static std::vector<Window*>::iterator iterateWindows(size_t &num);
  protected:
  static std::vector<Window*> windows;
  Window() = default;
  };

  class export_def Time {
  public:
	  static uint64_t frame();
	  static uint64_t delta();
	  static uint64_t lastFrame();
	  static uint64_t launchTime();
	  static uint64_t nowNs();
	  static uint64_t nowMs();
  private:
	  Time() = delete;
  };
  
}
