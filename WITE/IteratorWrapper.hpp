#pragma once

#include "DEBUG.hpp"

namespace WITE::Collections {

  template<class T> class IteratorWrapper {
  private:
    T startI, currentI, endI;
  public:
    IteratorWrapper() : startI(), currentI(), endI() {};//already completed iterator, all these of the same type match
    IteratorWrapper(T start, T end) : startI(start), currentI(start), endI(end) {};
    template<class U> requires std::negation<std::is_same<U, IteratorWrapper<T>>>::value
    IteratorWrapper(U& collection) : IteratorWrapper(collection.begin(), collection.end()) {};
    IteratorWrapper(const IteratorWrapper<T>& src) : startI(src.startI), currentI(src.currentI), endI(src.endI) {};
    template<typename v = void> requires std::is_pointer<T>::value
    IteratorWrapper(T start, size_t len) : startI(start), currentI(start), endI(start + len) {}

    inline void reset() { currentI = startI; };
    inline operator bool() const { return currentI != endI; };
    typename std::conditional<std::is_pointer_v<T>,
			      typename std::add_lvalue_reference<std::remove_pointer_t<T>>::type,
			      decltype(*currentI)>::type
    inline operator*() const { return *currentI; };
    inline auto& operator->() const { return *currentI; };
    inline operator const T&() const { return currentI; };
    inline bool operator==(const IteratorWrapper<T>& r) const { return (!r && !*this) || r.currentI == this->currentI; };

    //inline bool operator!=(const IteratorWrapper<T>& r) { return !operator==(r); };
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
    inline size_t count() {
      size_t ret = 0;
      IteratorWrapper<T> clone(*this);
      while(clone++) ret++;
      return ret;
    };
  };

}
