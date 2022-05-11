namespace WITE::Utils {
  template <typename T, typename U> size_t member_offset(U T::*member) {
    //image that there was a T sitting on NULL, what would the address of member be? That's the offset.
    return reinterpret_cast<size_t>(&(static_cast<T*>(nullptr)->*member));
  }
}
