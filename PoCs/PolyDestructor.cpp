#include <iostream>

class A {
public:
  int* data;
  A(int i) : data(new int()) {
    *data = i;
    std::cout << "Set to " << i << std::endl;
  };
  virtual ~A() {
    std::cout << "Destructor fired: " << (*data) << std::endl;
    delete data;
  };
};

class B : public A {
public:
  B() : A(6) {};
  virtual ~B() {
    std::cout << "Destructor B fired" << std::endl;
  };
};

int main(int argc, char** argv) {
  A* a = new B();
  delete a;
};

