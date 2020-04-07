#pragma once

#include "stdafx.h"
#include "constants.h"

namespace WITE {
  
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
    return new CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return new CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };
  
#define typedefCB(name, ...) typedef WITE::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ;

  typedefCB(rawDataSource, int, unsigned char*, size_t)

  class export_def ShaderResource {
  public:
  virtual void load(rawDataSource) = 0;
  virtual unsigned char* map() = 0;
  virtual void unmap() = 0;
  virtual size_t getSize() = 0;
  //virtual BackedBuffer* getBuffer() = 0;
  virtual ~ShaderResource() = default;
  };

  class export_def BBox3D {
  public:
  union {
    struct { float minx, miny, minz, maxx, maxy, maxz, centerx, centery, centerz; };
    struct { glm::vec3 min, max, center; };//sizeof glm::vec3 is not clear, may contain functions. TODO test this.
  };
  BBox3D(float minx = 0, float maxx = 0, float miny = 0, float maxy = 0, float minz = 0, float maxz = 0) :
  BBox3D(glm::vec3(minx, miny, minz), glm::vec3(maxx, maxy, maxz)) {}
  BBox3D(glm::vec3 min, glm::vec3 max) : min(min), max(max), center((min + max) * 0.5f) {}
  inline float width2D() { return maxx - minx; };
  inline float height2D() { return maxy - miny; };
  };

  class export_def IntBox3D {
  public:
  union {
    uint64_t comp[1];
    struct { uint64_t minx, miny, minz, maxx, maxy, maxz, centerx, centery, centerz, width, height, depth; };
  };
  IntBox3D(uint64_t minx = 0, uint64_t maxx = 0, uint64_t miny = 0, uint64_t maxy = 0, uint64_t minz = 0, uint64_t maxz = 0) :
  minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz),
  centerx((maxx+minx)/2), centery((maxy + miny) / 2), centerz((maxz + minz) / 2),
  width(maxx - minx), height(maxy - miny), depth(maxz - minz) {};
  inline bool operator==(IntBox3D& o) { return memcmp((void*)comp, (void*)o.comp, 6 * sizeof(minx)) == 0; };
  inline bool sameSize(IntBox3D& o) { return o.maxx - o.minx == maxx - minx && o.maxy - o.miny == maxy - miny && o.maxz - o.minz == maxz - minz; };
  };

  class export_def Shader {
  public:
  struct resourceLayoutEntry {
    size_t type, stage, perInstance;
    void* moreData;
  };
  static Shader* make(const char* filepathWildcard, struct shaderResourceLayoutEntry*, size_t resources);//static collection contains all
  static Shader* make(const char** filepath, size_t files, struct shaderResourceLayoutEntry*, size_t resources);//static collection contains all
  virtual ~Shader() = default;
  };

  class export_def MeshSource {//mesh source can redirect to file load or cpu procedure, but should not store any mesh data. Mesh does that, per LOD.
  public:
  virtual uint32_t populateMeshCPU(void*, uint64_t maxVerts, const glm::dvec3* viewOrigin) = 0;//returns number of verts used
  virtual Shader* getComputeMesh(void) { return NULL; };
  virtual BBox3D* getBbox(BBox3D* out = NULL) = 0;//not transformed
  virtual ~MeshSource() = default;
  };

  class StaticMesh : public MeshSource {//for debug or simple things only, don't waste host ram on large ones
  public:
    StaticMesh(BBox3D box, void* data, uint32_t size) : box(box), data(data), size(size) {}
    uint32_t populateMeshCPU(void* out, uint64_t maxVerts, const glm::dvec3* viewOrigin) {
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
    void* data;
    uint32_t size;
  };

  class Window;
  typedef uint8_t renderLayerIdx;
  typedef uint64_t renderLayerMask;
  
  class export_def Camera{
  public:
  static Camera* make(Window*, IntBox3D);//window owns camera object
  virtual ~Camera() = default;
  virtual void resize(IntBox3D) = 0;
  virtual float getPixelTangent() = 0;
  virtual float approxScreenArea(BBox3D*) = 0;
  virtual glm::dvec3 getLocation() = 0;
  virtual void setLocation(glm::dvec3) = 0;
  virtual Window* getWindow() = 0;
  virtual IntBox3D getScreenRect() = 0;
  virtual void setFov(double) = 0;
  virtual double getFov() = 0;
  virtual bool appliesOnLayer(renderLayerIdx i) = 0;
  };

  class export_def Window {
  public:
  virtual ~Window() = default;
  virtual size_t getCameraCount() = 0;
  virtual void setSize(uint32_t width, uint32_t height) = 0;
  virtual void setBounds(IntBox3D) = 0;
  virtual void setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
  virtual Camera* addCamera(IntBox3D) = 0;
  virtual Camera* getCamera(size_t idx) = 0;
  static std::unique_ptr<Window> make(size_t display = 0);
  protected:
  static std::vector<Window*> windows;
  };
  
}
