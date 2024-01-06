#include <iostream>
#include <tuple>
#include <stdint.h>
#include <type_traits>
#include <iterator>
#include <memory>
#include <cstring>
#include <string>
#include <limits>

#include "../WITE/Callback.hpp"

class Integer {
public:
  typedefCB(oneInOneOut, int, int);
  typedefCB(twoInOneOut, int, int, int);
  int value;
  Integer(int v) : value(v) {};
  int plus(int o) {
    return value + o;
  };
  int plus(int a, int b, int c) {
    return value + a + b + c;
  };
  static int simpleSum(int a, int b) {
    return a + b;
  };
};

#define assert(cond) if(!(cond)) std::cerr << "Error " << __FILE__ << ":" << __LINE__

int main(int argc, char** argv) {
  std::cout << "Running callback tests" << std::endl;

  Integer::twoInOneOut sumOfTwo = Integer::twoInOneOut_F::make(&Integer::simpleSum);//no object instance
  assert(sumOfTwo->call(5, 6) == 11);

  Integer seven(7);
  Integer::oneInOneOut sevenPlus = Integer::oneInOneOut_F::make<Integer>(&seven, &Integer::plus);
  assert(sevenPlus->call(12) == 19);

  Integer::twoInOneOut seventeenPlusDouble = Integer::twoInOneOut_F::make<Integer, int>(&seven, 10, &Integer::plus);
  //provide the 1st arg here, and the other 2 when called
  assert(seventeenPlusDouble->call(52, 48) == 117);

  Integer::oneInOneOut seventeenPlusSingle = Integer::oneInOneOut_F::make<Integer, int, int>(&seven, 6, 4, &Integer::plus);
  //provide the 1st 2 args here, and the other 1 when called
  assert(seventeenPlusSingle->call(50) == 67);//67
}

