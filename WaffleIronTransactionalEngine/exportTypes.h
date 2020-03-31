#pragma once

#include "stdafx.h"
#include "constants.h"
#include "AtomicLinkedList.h"
#include "Database.h"

#ifdef _WIN32
#define export __declspec(dllexport)
#define wintypename typename
typedef HANDLE FileHandle;
#else
//TODO
#define wintypename
#endif

namespace WITE {

  struct strcmp_t {
    bool operator() (const std::string& a, const std::string& b) const { return a.compare(b); }
  };

  template<class RET, class... RArgs> class export Callback_t {
  public:
  virtual RET call(RArgs... rargs) = 0;
  virtual ~Callback_t() = default;
  template<class U, class... CArgs> static Callback_t<RET, RArgs...>* make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
  };

  template<class RET, class... RArgs> class export CallbackFactory {
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

  class export ShaderResource {
  public:
  virtual void load(rawDataSource) = 0;
  virtual unsigned char* map() = 0;
  virtual void unmap() = 0;
  virtual size_t getSize() = 0;
  virtual VBackedBuffer* getBuffer() = 0;
  virtual ~ShaderResource() = default;
  };

  class export BBox3D {
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

  class export IntBox3D {
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

  class export VCamera{
  public:
  static VCamera* make();
  virtual ~VCamera() = default;
  virtual float getPixelTangent() = 0;
  virtual float approxScreenArea(BBox3D*) = 0;
  virtual glm::vec3* getLocation() = 0;
  virtual Window* getWindow() = 0;
  virtual IntBox3D getScreenRect() = 0;
  virtual void setTarget(VWindow*, IntBox3D*) = 0;
  virtual bool appliesOnLayer(renderLayerIdx i) = 0;
  virtual GPU* getGPU() = 0;
  virtual glm::mat4* getProjectionMatrix();
  };

  class export VShader {
  public:
  typedef std::vector<ShaderResource> InstanceResources;
  static VShader* make(const char* filepathWildcard, size_t resources, struct shaderResourceLayoutEntry*);
  static VShader* make(const char** filepath, size_t files, size_t resources, struct shaderResourceLayoutEntry*);
  virtual ~VShader() = default;
  virtual void render(VCamera* observer) = 0;
  virtual void ensureResources(GPU*) = 0;
  };

  class export MeshSource {//mesh source can redirect to file load or cpu procedure, but should not store any mesh data. VMesh does that, per LOD.
  public:
  virtual uint32_t populateMeshCPU(void*, uint32_t maxVerts, glm::vec3* viewOrigin) = 0;//returns number of verts used
  virtual Shader* getComputeMesh(void) { return NULL; };
  virtual BBox3D* getBbox(BBox3D* out = NULL) = 0;//not transformed
  virtual ~MeshSource() = default;
  };

  class export Window {
  public:
  virtual ~Window() = default;
  virtual void render() = 0;
  virtual bool isRenderDone() = 0;
  static void renderAll();
  static bool areRendersDone();
  static std::unique_ptr<VWindow> make(size_t display = 0);
  protected:
  static std::vector<Window*> windows;
  };

  class export USyncLock {
  public:
  virtual ~USyncLock() = default;
  virtual void WaitForLock() = 0;
  virtual void ReleaseLock() = 0;
  virtual uint64_t getId() = 0;
  virtual bool isLocked() = 0;
  virtual inline void yield() = 0;
  static std::unique_ptr<USyncLock> make();
  };
  class export UScopeLock {
  public:
  static std::unique_ptr<UScopeLock> make(USyncLock * lock, uint64_t timeoutNs = BASIC_LOCK_TIMEOUT);
  virtual ~UScopeLock() = default;
  virtual inline void yield() = 0;
  };
  class export UDataLock {
  public:
  static std::unique_ptr<UDataLock> make();
  virtual ~UDataLock() = default;
  virtual bool obtainReadLock(uint64_t timeoutNs = BASIC_LOCK_TIMEOUT) = 0;
  virtual void releaseReadLock() = 0;
  virtual bool obtainWriteLock(uint64_t timeoutNs = BASIC_LOCK_TIMEOUT) = 0;
  virtual void releaseWriteLock() = 0;
  virtual bool escalateToWrite(uint64_t timeoutNs = BASIC_LOCK_TIMEOUT) = 0;
  virtual void dropWrite() = 0;
  };
  class export UScopeReadLock {
  public:
  static std::unique_ptr<UScopeReadLock> make(UDataLock * lock, uint64_t timeoutNs = BASIC_LOCK_TIMEOUT);
  virtual ~UScopeReadLock() = default;
  };
  class export UScopeWriteLock {
  public:
  static std::unique_ptr<UScopeWriteLock> make(UDataLock * lock, uint64_t timeoutNs = BASIC_LOCK_TIMEOUT);
  virtual ~UScopeWriteLock() = default;
  };
  
}
