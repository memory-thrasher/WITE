#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WITE::Util {

  template <typename T, typename U> size_t member_offset(U T::*member) {
    //imagine that there was a T sitting on NULL, what would the address of member be? That's the offset.
    return reinterpret_cast<size_t>(&(static_cast<T*>(nullptr)->*member));
  }

  // template<class T> struct declval_helper { static T val; };
  // template<class T, class U, U T::*F> struct offset_helper {
  //   using TV = declval_helper<T>;
  //   uint8_t bytes[(uint8_t*)&(TV::val.*F) - (uint8_t*)&TV::val];
  // };
  // template<class T, class U, U T::*F> consteval int offset_of() {
  //   return sizeof(offset_helper<T, U, F>::bytes);
  // };

  inline uint64_t reverse(uint64_t i) {
    i = i >> 32 | i << 32;
    i = (i & 0xFFFF0000FFFF0000) >> 16 | (i & 0x0000FFFF0000FFFF) << 16;
    i = (i & 0xFF00FF00FF00FF00) >> 8  | (i & 0x00FF00FF00FF00FF) << 8;
    i = (i & 0xF0F0F0F0F0F0F0F0) >> 4  | (i & 0x0F0F0F0F0F0F0F0F) << 4;
    i = (i & 0xCCCCCCCCCCCCCCCC) >> 2  | (i & 0x3333333333333333) << 2;
    i = (i & 0xAAAAAAAAAAAAAAAA) >> 1  | (i & 0x5555555555555555) << 1;
    return i;
  };

}
