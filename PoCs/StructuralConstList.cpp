// #include <initializer_list>
// #include <memory>

// template<class T, class Alloc = std::allocator<T>> class StructuralConstList {
// public:
//   using AllocTraits = std::allocator_traits<Alloc>;
//   Alloc alloc;
//   const Alloc::size_type len;
//   AllocTraits::pointer data;
//   constexpr StructuralConstList(std::initializer_list<T> list) :
//     len(std::distance(list.begin(), list.end())),
//     data(AllocTraits::allocate(alloc, len))
//   {
//     auto start = list.begin();
//     while(start != list.end())
//       std::allocator_traits<Alloc>::construct(alloc, data, *start++);
//   };
//   constexpr ~StructuralConstList() {
//     AllocTraits::deallocate(alloc, data, len);
//   };
//   constexpr inline auto begin() const { return data; };
//   constexpr inline auto end() const { return data + len; };
// };

  //  static constexpr StructuralConstList<int> ints = { 7, 12, 49, 157 };

#include <vector>
#include <algorithm>

constexpr int f() {
    std::vector<int> v = {1, 2, 3};
    return v.size();
}

static_assert(f() == 3);

int main(int argc, char** argv) {
  return 0;
}


