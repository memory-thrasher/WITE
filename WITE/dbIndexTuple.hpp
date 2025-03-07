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

#include "dbIndex.hpp"

namespace WITE {

  template<size_t O, class A, class... REST> struct dbIndexTuple {//terminal case first

    dbIndexTuple(const std::filesystem::path&, bool) {};

  };

  template<size_t O, class A, class R, class... REST> struct dbIndexTuple<O, A, R, REST...> {

    static_assert(!std::is_same<A, R>::value, "whatever you were trying to do, there's a better way.");
    typedef std::remove_const<typename std::remove_reference<R>::type>::type T;
    //tuple may return a reference to a data field for efficiency, which is to be copied into the index
    dbIndex<T> idx;
    dbIndexTuple<O+1, A, REST...> rest;

    dbIndexTuple(const std::filesystem::path& basedir, bool clobber) :
      idx(basedir / std::filesystem::path(std::format("{}_idx_{}.wdb", A::typeId, O)), clobber),
      rest(basedir, clobber)
    {};

    template<size_t I> inline auto& get() {
      if constexpr(I == O)
	return idx;
      else
	return rest.template get<I>();
    };

    template<class T> inline auto& get() {
      if constexpr(std::is_same<R, T>::value)
	return idx;
      else
	return rest.template get<T>();
    };

    inline auto* operator->() {
      return &idx;
    };

    inline auto& operator*() {
      return idx;
    };

    inline auto& next() {
      return rest;
    };

  };

  template<class A, class... Args> extern dbIndexTuple<0, A, Args...> dbIndexTupleTypeFromStdTuple(std::tuple<Args...>);
  //extern is a lie: this function has no body or implementation anywhere, and is never actually called. It is only used to deduce a std::tuple type into the corresponding dbIndexTuple type by automatic parameter deduction

  template<class T> struct dbIndexTupleFor {//empty default
    static constexpr bool exists = false;
    dbIndexTupleFor(const std::filesystem::path&, bool) {};
  };

  template<class T> requires requires(uint64_t oid, const T& t, void* db) {
    { T::getIndexValues(oid, t, db) };
  }
  struct dbIndexTupleFor<T> {

    static constexpr bool exists = true;
    typedef decltype(T::getIndexValues(0, std::declval<const T&>(), NULL)) tpl;
    typedef decltype(dbIndexTupleTypeFromStdTuple<T>(std::declval<tpl>())) type;

    type indices;

    dbIndexTupleFor(const std::filesystem::path& basedir, bool clobber) :
      indices(basedir, clobber) {};

    inline type* operator->() {
      return &indices;
    };

    inline type& operator*() {
      return indices;
    };

  };

}
