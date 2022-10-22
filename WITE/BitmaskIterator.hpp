namespace WITE::Collections {

  class BitmaskIterator {
  private:
    size_t i;
    const int64_t value;
  public:
    BitmaskIterator(int64_t value) : i(0), value(value) {
      while((value & (1 << i)) == 0 && i < 64) i++;
    };
    template<class T> BitmaskIterator(T value) : BitmaskIterator(static_cast<int64_t>(value)) {};
    BitmaskIterator(const BitmaskIterator& o) : i(o.i), value(o.value) {};
    BitmaskIterator() : BitmaskIterator(0) {};
    inline operator bool() const { return value >> i; };
    inline const size_t& operator*() const { return i; };
    inline BitmaskIterator operator++() {
      i++;
      while((value & (1 << i)) == 0 && i < 64) i++;
      return *this;
    };
    inline BitmaskIterator operator++(int) {
      auto old = *this;
      operator++();
      return old;
    };
    inline bool operator==(const BitmaskIterator& o) const { return (!*this && !o) || (i == o.i && value == o.value); };
  };

}
