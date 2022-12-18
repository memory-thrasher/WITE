#include <iostream>

#include "../WITE/LiteralJaggedList.hpp"

//constexpr auto count = WITE::Collections::countIlRecursive<4>({
defineLiteralJaggedList(int, 4, test,
    STDIL3(int) {
      {
	{ 4, 5, 7 },
	{ 2, 1 }
      }, {
	{},
	{ 47 }
      }
    }, {
      {
	{ 41 }
      }
    }
  );

int main(int argc, char** argv) {
  for(size_t i = 0;i < test.dataCount;i++)
    std::cout << "data[" << i << "] = " << test.data[i] << std::endl;
  std::cout << std::endl;
  for(size_t i = 0;i < test.indexCount;i++)
    std::cout << "indexes[" << i << "] = " << test.indexes[i] << std::endl;
  std::cout << "get 0, 1, 1, 0 = " << test.get(0, 1, 1, 0) << std::endl;
}

