#pragma once

#include "stdafx.h"
#include "constants.h"
#include "AtomicLinkedList.h"
#include "Database.h"

namespace WITE {

  struct strcmp_t {
    bool operator() (const std::string& a, const std::string& b) const { return a.compare(b); }
  };

  template<class RET, class... RArgs> class export_def Callback_t {
  public:
  virtual RET call(RArgs... rargs) = 0;
  virtual ~Callback_t() = default;
  template<class U, class... CArgs> static Callback_t<RET, RArgs...>* make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
  };

  template<class RET, class... RArgs> class export_def CallbackFactory {
  private:
  template<class T, class... CArgs> class Callback : public Callback_t<RET, RArgs...> {
  private:
    T * owner;
    RET(T::*x)(CArgs..., RArgs...);
    std::tuple<CArgs...> cargs;
    RET call(RArgs... rargs) {
      return (*owner.*(x))(std::get<CArgs>(cargs)..., rargs...);
    };
  public:
    Callback(T* t, RET(T::*x)(CArgs..., RArgs...), CArgs... pda);
    ~Callback() {};
  };
  public:
  template<class U, class... CArgs> static Callback_t<RET, RArgs...>* make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
  };
  template<class RET2, class... RArgs2> template<class T2, class... CArgs2>
  CallbackFactory<RET2, RArgs2...>::Callback<T2, CArgs2...>::Callback(T2* t, RET2(T2::*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), owner(t), cargs(std::forward<CArgs2>(pda)...) {}
  template<class RET, class... RArgs> template<class U, class... CArgs>
  Callback_t<RET, RArgs...>* CallbackFactory<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  }
  template<class RET, class... RArgs> template<class U, class... CArgs>
  Callback_t<RET, RArgs...>* Callback_t<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return CallbackFactory<RET, RArgs...>::make<U, CArgs>(owner, cargs, func);
  }//new alias so typedefs can be used to make objects without redundant signature declaration

  typedef class Callback_t<int, unsigned char*, size_t>* rawDataSource;//size_t = maxsize for write, return error code or 0

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
    uint64_t comp[];
    struct { uint64_t minx, miny, minz, maxx, maxy, maxz, centerx, centery, centerz, width, height, depth; };
  }
  IntBox3D(uint64_t minx = 0, uint64_t maxx = 0, uint64_t miny = 0, uint64_t maxy = 0, uint64_t minz = 0, uint64_t maxz = 0) :
  minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz),
  centerx((maxx+minx)/2), centery((maxy + miny) / 2), centerz((maxz + minz) / 2),
  width(maxx - minx), height(maxy - miny), depth(maxz - minz) {}
  inline bool operator==(IntBox3D& o) { return memcmp((void*)comp, (void*)o.comp); };
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
  virtual void ensureResources(GPU*) = 0;
  };

  class export_def MeshSource {//mesh source can redirect to file load or cpu procedure, but should not store any mesh data. Mesh does that, per LOD.
  public:
  virtual uint32_t populateMeshCPU(void*, uint32_t maxVerts, glm::vec3* viewOrigin) = 0;//returns number of verts used
  virtual Shader* getComputeMesh(void) { return NULL; };
  virtual BBox3D* getBbox(BBox3D* out = NULL) = 0;//not transformed
  virtual ~MeshSource() = default;
  };

  class export_def Camera{
  public:
  static Camera* make(Window*, IntBox3D);//window owns camera object
  virtual ~Camera() = default;
  virtual void resize(IntBox3D) = 0;
  virtual float getPixelTangent() = 0;
  virtual float approxScreenArea(BBox3D*) = 0;
  virtual glm::vec3* getLocation() = 0;
  virtual Window* getWindow() = 0;
  virtual IntBox3D getScreenRect() = 0;
  virtual void setFov(double) = 0;
  virtual double getFov() = 0;
  virtual bool appliesOnLayer(renderLayerIdx i) = 0;
  virtual GPU* getGPU() = 0;
  };

  class export_def Window {
  public:
  virtual ~Window() = default;
  virtual std::vector<Camera*> iterateCameras(size_t &num) = 0;
  virtual void setSize(uint32_t width, uint32_t height) = 0;
  virtual void setBounds(IntBox3D) = 0;
  virtual void setLocation(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
  virtual Camera* addCamera(IntBox3D) = 0;
  virtual Camerar* getCamera(size_t idx) = 0;
  static std::unique_ptr<Window> make(size_t display = 0);
  protected:
  static std::vector<Window*> windows;
  };

  class export_def SyncLock {
  public:
  virtual ~SyncLock() = default;
  virtual void WaitForLock() = 0;
  virtual void ReleaseLock() = 0;
  virtual uint64_t getId() = 0;
  virtual bool isLocked() = 0;
  virtual void yield() = 0;
  static std::unique_ptr<USyncLock> make();
  };
  class export_def ScopeLock {
  public:
  static std::unique_ptr<ScopeLock> make(SyncLock * lock);
  virtual ~ScopeLock() = default;
  virtual void yield() = 0;
  };
  
}
