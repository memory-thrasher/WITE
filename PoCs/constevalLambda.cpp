#include <iostream>

struct Bar {
  int x, y;
};

template<Bar B> struct Foo {
  template<class L> static consteval int defer(L l) {
    return l(B);
  };
};

consteval int cef(int x, int y) {
  return x + y/2;
};

template<Bar B> struct barDataBundle {
  // static constexpr int filteredNumbersLength = Foo<B>::defer([](Bar b) consteval { return cef(6, b.x); });
  // int filteredNumbers[filteredNumbersLength];
  static consteval int filteredNumbers2Length() {
    return Foo<B>::defer([](Bar b) consteval { return cef(6, b.x); });
  };
  int filteredNumbers2[filteredNumbers2Length()];
};

int main(int argc, char** argv) {
  barDataBundle<Bar {4, 5}> bdb;
  std::cout << sizeof(bdb.filteredNumbers2) << std::endl;
};

