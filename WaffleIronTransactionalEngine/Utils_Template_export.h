#pragma once

namespace WITE {

  template<class T> struct remove_array { typedef T type; };
  template<class T> struct remove_array<T[]> { typedef T type; };
  template<class T, size_t N> struct remove_array<T[N]> { typedef T type; };
  template<class T> struct remove_array_deep { typedef T type; };
  template<class T> struct remove_array_deep<T[]> { typedef typename remove_array_deep<T>::type type; };
  template<class T, size_t N> struct remove_array_deep<T[N]> { typedef typename remove_array_deep<T>::type type; };

  template<class T, class compare = std::less<T>> static constexpr bool compareArrays(T a[], T b[], size_t n, compare C = compare(), std::less<size_t> D = std::less<size_t>()) {
    if(n == 0) return true;
    if constexpr(C(a[0], b[0]) || C(b[0], a[0])) return false;
    return compareArrays(a+1, b+1, n-1, C);
  }

  template<class T, class compare = std::less<T>> static constexpr bool arrayContains(T a[], size_t size, T element, compare C = compare()) {
    if constexpr(!C(a[0], element) && !C(element, a[0])) return true;
    if(size) return arrayContains(a + 1, size - 1, element, C);
    return false;
  }

  template<class T, size_t size> static constexpr std::array<T, size + 1> arrayAppend(std::array<T, size> a, T e) {
    std::array<T, size + 1> ret;
    std::copy(a.begin(), a.end(), ret.begin());
    ret[size] = e;
    return ret;
  }

}
