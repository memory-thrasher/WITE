#include <initializer_list>

namespace WITE::Collections {

  template<class T> constexpr size_t countIL(const std::initializer_list<T> il) { return std::distance(il.begin(), il.end()); };

#define defineLiteralList(T, NOM, ...) constexpr ::std::array<T, ::WITE::Collections::countIL({ __VA_ARGS__ })> NOM = { __VA_ARGS__ }

};
