#include <array>
#include <algorithm>
#include <initializer_list>

#include "LiteralList.hpp" //for countIL

namespace WITE::Collections {

  template<class T, size_t N, class Compare = std::less<T>> class LiteralSet {
  public:
    const ::std::array<T, N> data;//sorted
    static constexpr size_t len = N;

    constexpr LiteralSet() = delete;
    constexpr LiteralSet(const LiteralSet<T, N> o) : data(o.data) {};
    constexpr LiteralSet(const std::initializer_list<T> in) {
      std::array<T, N> temp(in);//temporary is ok
      std::sort(temp.begin(), temp.end(), Compare());
      data = temp;//single assignment required by const
    };
    constexpr ~LiteralSet() = default;

    constexpr inline bool contains(const T& t) const {
      return std::binary_search(data.begin(), data.end(), t);
    };

  };

#define defineLiteralSet(T, NOM, ...) constexpr ::WITE::Collections::LiteralSet<T, ::WITE::Collections::countIL({ __VA_ARGS__ })> NOM = { __VA_ARGS__ };

};
