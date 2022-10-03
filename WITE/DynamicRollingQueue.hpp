#include <queue>
#include <deque>

namespace WITE::Collections {

  template<typename T, class Allocator = std::allocator<T>> class DynamicRollingQueue {
  private:
    std::queue<T, std::deque<T, Allocator>> data;//Maybe array of T would be faster?
    std::queue<T*> free;
    std::queue<T*> allocated;
  public:
    T* peek() {
      return allocated.empty() ? NULL : allocated.front();
    };
    T* push(T* src) {
      T* ret;
      if(free.empty()) {
	ret = &data.emplace();
      } else {
	ret = free.front();
	free.pop();
      }
      allocated.push(ret);
      *ret = *src;
      return ret;
    };
    bool pop(T* out) {
      if(allocated.empty()) return false;
      T* ret = allocated.front();
      *out = *ret;
      allocated.pop();
      free.push(ret);
      return true;
    };
    inline size_t count() { return allocated.size(); };
    inline size_t freeSpace() { return free.size(); };
    bool popIf(T* out, Util::CallbackPtr<bool, const T*> condition) {
      if(allocated.empty()) return false;
      T* ret = allocated.front();
      if(!condition(ret)) return false;
      *out = *ret;
      allocated.pop();
      free.push(ret);
      return true;
    }
  };

}
