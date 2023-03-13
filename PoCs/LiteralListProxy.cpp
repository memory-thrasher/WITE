#include <array>
#include <memory>
#include <iostream>

template<class T> constexpr size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

template<typename T> struct LiteralList {
  const T* data;
  const size_t len;
  constexpr LiteralList() = default;
  constexpr LiteralList(const T* data, const size_t len) : data(data), len(len) {};
  template<size_t LEN> constexpr LiteralList(const std::array<T, LEN>& o) : LiteralList(o.data(), LEN) {};
  constexpr ~LiteralList() = default;
};

#define defineLiteralList(T, NOM, ...) constexpr std::array<T, ::countIL({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

defineLiteralList(int, foo, 1, 2, 5);

template<LiteralList<int> L> struct bar {
  template<size_t i> static void print() {
    constexpr size_t len = L.data[i];
    int data[len];
    for(size_t j = 0;j < len;j++)
      data[j] = L.data[j%L.len];
    for(size_t j = 0;j < len;j++)
      std::cout << data[j] << std::endl;
  };
};

int main(int argc, char** argv) {
  bar<foo>::print<2>();
}


