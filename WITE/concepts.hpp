/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

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
