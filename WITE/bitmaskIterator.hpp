#pragma once

namespace WITE {

  class bitmaskIterator {
  private:
    size_t i;
    const int64_t value;
  public:
    bitmaskIterator(int64_t value) : i(0), value(value) {
      while((value & (1 << i)) == 0 && (value >> i) && i < sizeof(value) * 8) i++;
    };
    template<class T> bitmaskIterator(T value) : bitmaskIterator(static_cast<int64_t>(value)) {};
    bitmaskIterator(const bitmaskIterator& o) : i(o.i), value(o.value) {};
    bitmaskIterator() : bitmaskIterator(0) {};
    inline operator bool() const { return value >> i && i < sizeof(value) * 8; };//strange behavior: (1 >> 64) = 1
    inline const size_t& operator*() const { return i; };
    template<typename T> inline T asMask() const { return static_cast<T>(1 << i); };
    inline bitmaskIterator operator++() {
      i++;
      while((value & (1 << i)) == 0 && i < sizeof(value) * 8) i++;
      return *this;
    };
    inline bitmaskIterator operator++(int) {
      auto old = *this;
      operator++();
      return old;
    };
    inline bool operator==(const bitmaskIterator& o) const { return (!*this && !o) || (i == o.i && value == o.value); };
    inline bitmaskIterator begin() const { return *this; };
    inline bitmaskIterator end() const { return 0; };
  };

}
