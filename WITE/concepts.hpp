#pragma once

#include <type_traits>

namespace WITE {

  template<typename S, typename T> concept indexable_to = requires(S s, size_t i) { { s[i] } -> std::convertible_to<T>; };

  template<typename L, typename T> concept lambda_indexer_to = requires(L l, size_t i) { { l(i) } -> std::convertible_to<T>; };

  template<typename I, typename T> concept iterator = requires(I iter) {
    { ++iter } -> std::convertible_to<I>;
    { *iter } -> std::convertible_to<T>;
  };

  template<typename C, typename T> concept iterable_unindexable = requires(C c, size_t i) {
    { *c.begin() } -> std::convertible_to<T>;
    requires (!indexable_to<C, T>);
  };

  template<typename T, typename L> concept predicate = requires(L l, T t) { { l(t) } -> std::same_as<bool>; };

  template<typename I, typename T> concept iterator_not_ptr = requires(I iter) {
    requires iterator<I, T>;
    requires std::negation<std::is_pointer<I>>::value;
  };

};
