namespace WITE::Platform {

  class Thread {
  public:
    typedefCB(threadEntry_t, void);
    static uint32_t getCurrentTid();//returns engine thread index, NOT system tid. Can be used to index.
    static void init();
    static void initThisThread(threadEntry_t entry = NULL);
    static void spawnThread(threadEntry_t entry);
    static Thread* current();
    static void sleep(uint64_t ms = 0);
    uint32_t getTid();
    ~Thread();
  private:
    static std::atomic<uint32_t> seed;
    static ThreadResource<Thread> threads;
    threadEntry_t entry;
    uint32_t tid;
    Thread(threadEntry_t entry, uint32_t id);
    Thread();//dumby for allocation
  };

  template<class T> T* ThreadResource<T>::get() {
    return get(Thread::getCurrentTid());
  }

}
