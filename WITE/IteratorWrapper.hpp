#pragma once

namespace WITE::Collections {

  template<class T> class IteratorWrapper {
  private:
    T startI, currentI, endI;
  public:
    IteratorWrapper() : startI(), currentI(), endI() {}//already completed iterator, all these of the same type match
    IteratorWrapper(T start, T end) : startI(start), endI(end), currentI(start) {}
    template<class U> IteratorWrapper(U& collection) : IteratorWrapper(collection.begin(), collection.end()) {};
    inline void reset() { currentI = startI; };
    inline operator bool() { return currentI != endI; };
    inline auto operator*() const { return *currentI; };
    inline auto& operator->() const { return *currentI; };
    inline bool operator==(const IteratorWrapper<T>& r) { return (!r && !*this) || r.currentI == this->currentI; };
    inline bool operator!=(const IteratorWrapper<T>& r) { return !operator==(r); };
    inline IteratorWrapper operator++() {
      currentI++;
      return *this;
    };
    inline IteratorWrapper operator++(int) {
      auto old = *this;
      operator++();
      return old;
    };
    //begin and end so this can be the target of a foreach
    inline IteratorWrapper<T> begin() const { return *this; };
    inline IteratorWrapper<T> end() const { return IteratorWrapper<T>(); };
  };

}
