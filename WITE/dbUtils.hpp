/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <type_traits>
#include <concepts>

#include "threadPool.hpp"

namespace WITE {

  template<class T> struct has_update : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { {T::update(oid)} -> std::same_as<void>; }
  struct has_update<T> : public std::true_type {};

  template<class T> struct has_allocated : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { {T::allocated(oid)} -> std::same_as<void>; }
  struct has_allocated<T> : public std::true_type {};

  template<class T> struct has_freed : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { {T::freed(oid)} -> std::same_as<void>; }
  struct has_freed<T> : public std::true_type {};

  template<class T> struct has_spunUp : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { {T::spunUp(oid)} -> std::same_as<void>; }
  struct has_spunUp<T> : public std::true_type {};

  template<class T> struct has_spunDown : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { {T::spunDown(oid)} -> std::same_as<void>; }
  struct has_spunDown<T> : public std::true_type {};

  //so we don't have to malloc up a new callbackPtr for every object being updated, reuse the callback object and store the oid in jobData
  template<class T, void(*F)(uint64_t)> struct dbJobWrapper {
    static void cb(threadPool::jobData_t& jd) { F(jd[0]); };
    static constexpr threadPool::jobEntry_t_F::StaticCallback<> cbt = &cb;
    static constexpr threadPool::jobEntry_t_ce cbce = &cbt;
    threadPool::job_t j;
    dbJobWrapper(uint64_t oid, threadPool& tp) : j({ threadPool::jobEntry_t(cbce), { oid } }) {
      tp.submitJob(&j);
    };
  };

}
