#pragma once

namespace WITE {

  class export_def GPU {//these are created by init and may be present as args in callbacks
  public:
    ~GPU() = default;
    GPU(const GPU&) = delete;
    virtual size_t getIdx() = 0;
    static GPU* get(size_t idx);
  protected:
    GPU() = default;
  };

  template<class T, class D = std::default_delete<T>> class export_def GPUResource {//mostly thread safe, dirty read possible, most gpu stuff is kept on main thread
  private:
    GPUResource(const GPUResource&) = delete;
    void inline ensureExists(GPU* g) {
      size_t idx = g->getIdx();
      if(data.size() <= idx) {
        WITE::ScopeLock lock(&allocLock);
        while(data.size() <= idx) data.emplace_back(std::nullptr_t());
        if(!data[idx]) data[idx] = initer->call(g);
      }
    };
    void inline ensureExists(size_t idx) {
      if(data.size() <= idx) {
        GPU* g = GPU::get(idx);
        WITE::ScopeLock lock(&allocLock);
        while(data.size() <= idx) data.emplace_back(std::nullptr_t());
        if(!data[idx]) data[idx] = initer->call(g);
      }
    };
  public:
    typedef std::unique_ptr<T, D> UniqPtr;
    typedefCB(constructor, UniqPtr, GPU*);
    T* get(GPU* g) {
      ensureExists(g);
      return data[g->getIdx()].get();
    }
    T* get(size_t idx) {
      ensureExists(idx);
      return data[idx].get();
    }
    template<class U = T>
    typename std::enable_if<std::is_array<U>::value, typename WITE::remove_array<U>::type>::type&
      get(GPU* gpu, size_t idx) {
      ensureExists(gpu);
      return data[gpu->getIdx()][idx];
    }
    template<class U = T>
    typename std::enable_if<std::is_array<U>::value, typename WITE::remove_array<U>::type>::type&
      get(size_t gpu, size_t idx) {
      ensureExists(gpu);
      return data[gpu][idx];
    }
    template<class U = T>
    typename std::enable_if<std::is_array<U>::value, typename WITE::remove_array<U>::type>::type*
      getPtr(size_t gpu, size_t idx) {
      ensureExists(gpu);
      return &data[gpu][idx];
    }
    T* operator[](size_t idx) {
      ensureExists(idx);
      return data[idx].get();
    }
    GPUResource(constructor c) : data(), initer(c) {}
  private:
    std::vector<UniqPtr> data;
    constructor initer;
    WITE::SyncLock allocLock;
  };

}
