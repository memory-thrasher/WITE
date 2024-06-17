/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

namespace WITE {

  template<size_t start, size_t deleteCount, class... Tup> struct tuple_splice;

  template<size_t start, size_t deleteCount, class Head, class... Tail>
  struct tuple_splice<start, deleteCount, std::tuple<Head, Tail...>> {
    using rest = tuple_splice<start-1, deleteCount, std::tuple<Tail...>>;
    using Right = typename rest::Right;
    using Left = std::tuple<Head, typename rest::Left>;
    using Outer = std::tuple<Left, Right>;
  };

  template<size_t deleteCount, class Head, class... Tail>
  struct tuple_splice<1, deleteCount, std::tuple<Head, Tail...>> :
    tuple_splice<0, deleteCount, std::tuple<Tail...>> {
    using Left = Head;
  };

  template<size_t deleteCount, class Head, class... Tail>
  struct tuple_splice<0, deleteCount, std::tuple<Head, Tail...>> :
    tuple_splice<0, deleteCount-1, std::tuple<Tail...>> {
  };

  template<class... Tup> struct tuple_splice<0, 0, Tup...> {
    using Right = std::tuple<Tup...>;
  };

}
