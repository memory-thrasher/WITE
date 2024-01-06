#include <ranges>
#include <vector>
#include <iostream>

template<std::ranges::contiguous_range R> requires std::convertible_to<std::ranges::range_reference_t<R>, int> 
void foo(R& r) {
  std::cout << "contiguous of first element: " << r[0] << std::endl;
};

template<std::ranges::range R> requires std::convertible_to<std::ranges::range_reference_t<R>, int> 
  void foo(R& r) {
  std::cout << "noncontiguous of first elemet: " << *r.begin() << std::endl;
};

void foo(std::initializer_list<int> r) {
  foo<std::initializer_list<int>>(r);
};

int main()
{
    int x[4] {1};
    foo(x);
    std::vector<int> y;
    y.push_back(7);
    foo(y);
    foo({ 12 });
}
