#include <iostream>

#include "../WITE/LiteralJaggedList.hpp"
#include "../WITE/ConstexprString.hpp"

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

template< acceptLiteralJaggedList(int, 4, JL) > requires(JL.contains(5)) struct Foo {
  template<size_t l0> constexpr static size_t sum() {
    size_t ret = 0;
    for(auto l1 : JL.getIterator(l0))
      for(auto l2 : l1)
	for(auto l3 : l2)
	  ret += l3;
    return ret;
  };
  // static constexpr size_t s1 = ;
  uint8_t F[sum<0>()], S[sum<1>()];
  constexpr static size_t debugSize = 4096;
  template<size_t l0i> constexpr static inline const std::array<char, debugSize> getDebug() {
    char ret[debugSize] = { 0 };
    typename decltype(JL)::template iterator<0> l0 = JL.getIterator(l0i);
    WITE::Util::strcat(ret, "l0: "); WITE::Util::to_string(ret, l0.target); WITE::Util::strcat(ret, "\n");
    for(auto l1 : l0) {
      WITE::Util::strcat(ret, "  l1: "); WITE::Util::to_string(ret, l1.target); WITE::Util::strcat(ret, "\n");
      for(auto l2 : l1) {
	WITE::Util::strcat(ret, "    l2: "); WITE::Util::to_string(ret, l2.target); WITE::Util::strcat(ret, "\n");
	WITE::Util::strcat(ret, "      l2 begin: "); WITE::Util::to_string(ret, l2.begin().target); WITE::Util::strcat(ret, "\n");
	WITE::Util::strcat(ret, "      l2 end: "); WITE::Util::to_string(ret, l2.end().target); WITE::Util::strcat(ret, "\n");
	// auto l3 = l2.begin();
	// if(l3 != l2.end()) {
	//   WITE::Util::strcat(ret, "      l3: "); WITE::Util::to_string(ret, l3.target); WITE::Util::strcat(ret, "\n");
	//   l3++;
	// }
      }
    }
    std::array<char, debugSize> r2;
    std::copy(ret, ret + sizeof(ret), r2.begin());
    return r2;
  };
  constexpr static std::array<char, debugSize> debug = getDebug<1>();
};

using Bar = Foo< passLiteralJaggedList(test) >;

//TODO move this to tests
int main(int argc, char** argv) {
  for(size_t i = 0;i < test.dataCount;i++)
    std::cout << "data[" << i << "] = " << test.data[i] << std::endl;
  std::cout << std::endl;
  for(size_t i = 0;i < test.indexCount;i++)
    std::cout << "indexes[" << i << "] = " << test.indexes[i] << std::endl;
  std::cout << "get 0, 1, 1, 0 = " << test.get(0, 1, 1, 0) << std::endl << std::endl;
  for(size_t i = 0;i < test.layerBoundaries.size();i++)
    std::cout << "layerBoundaries[" << i << "] = " << test.layerBoundaries[i] << std::endl;
  std::cout << std::endl;
  for(auto l0 : test) {
    std::cout << "{" << std::endl;
    for(auto l1 : l0) {
      std::cout << "  {" << std::endl;
      for(auto l2 : l1) {
	std::cout << "    { ";
	for(auto l3 : l2) {
	  std::cout << l3 << " ";
	}
	std::cout << "}" << std::endl;;
      }
      std::cout << "  }" << std::endl;
    }
    std::cout << "}" << std::endl;
  }
  std::cout << Bar::debug.data() << std::endl;
  std::cout << "L1: " << sizeof(Bar::F) << std::endl;
  std::cout << "L2: " << sizeof(Bar::S) << std::endl;
}

