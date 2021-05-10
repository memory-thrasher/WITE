#pragma once

namespace WITE {

  template<class T> struct remove_array { typedef T type; };
  template<class T> struct remove_array<T[]> { typedef T type; };
  template<class T, size_t N> struct remove_array<T[N]> { typedef T type; };
  template<class T> struct remove_array_deep { typedef T type; };
  template<class T> struct remove_array_deep<T[]> { typedef wintypename remove_array_deep<T>::type type; };
  template<class T, size_t N> struct remove_array_deep<T[N]> { typedef wintypename remove_array_deep<T>::type type; };

  template<class T, class compare = std::less<T>> static constexpr bool compareArrays(T a[], T b[], size_t n, compare C = compare()) const {
    constexpr if(C(a[0], b[0]) || C(b[0], a[0])) return false;
    constexpr if(n) return compareArrays(a+1, b+1, n-1, C);
    return true;
  }

  template<class T, class compare = std::less<T>> static constexpr bool arrayContains(T a[], size_t size, T element, compare C = compare()) const {
    constexpr if(!C(a[0], element) && !C(element, a[0])) return true;
    constexpr if(size) return arrayContains(a + 1, size - 1, element, C);
    return false;
  }

  template<class T, size_t size, typename U = T[size + 1]> static constexpr U arrayAppend(T[size] a, T e) {
    U ret;
    constexpr for(size_t i = 0;i < size;i++)
      ret[i] = a[i];
    ret[size] = e;
    return ret;
  }

}
