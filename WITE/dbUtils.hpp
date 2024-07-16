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
  template<class T> requires requires(uint64_t oid, void* db) { {T::update(oid, db)} -> std::same_as<void>; }
  struct has_update<T> : public std::true_type {};

  template<class T> struct has_allocated : public std::false_type {};
  template<class T> requires requires(uint64_t oid, void* db) { {T::allocated(oid, db)} -> std::same_as<void>; }
  struct has_allocated<T> : public std::true_type {};

  template<class T> struct has_freed : public std::false_type {};
  template<class T> requires requires(uint64_t oid, void* db) { {T::freed(oid, db)} -> std::same_as<void>; }
  struct has_freed<T> : public std::true_type {};

  template<class T> struct has_spunUp : public std::false_type {};
  template<class T> requires requires(uint64_t oid, void* db) { {T::spunUp(oid, db)} -> std::same_as<void>; }
  struct has_spunUp<T> : public std::true_type {};

  template<class T> struct has_spunDown : public std::false_type {};
  template<class T> requires requires(uint64_t oid, void* db) { {T::spunDown(oid, db)} -> std::same_as<void>; }
  struct has_spunDown<T> : public std::true_type {};

  //so we don't have to malloc up a new callbackPtr for every object being updated, reuse the callback object and store the oid in jobData
  template<class T, void(*F)(uint64_t, void*)> struct dbJobWrapper {
    static void cb(threadPool::jobData_t& jd) { F(jd[0], reinterpret_cast<void*>(jd[1])); };
    static_assert(sizeof(void*) <= sizeof(uint64_t));
    static constexpr threadPool::jobEntry_t_F::StaticCallback<> cbt = &cb;
    static constexpr threadPool::jobEntry_t_ce cbce = &cbt;
    threadPool::job_t j;
    dbJobWrapper(uint64_t oid, void* db, threadPool& tp) :
      j({ threadPool::jobEntry_t(cbce), { oid, reinterpret_cast<uint64_t>(db) } }) {
      tp.submitJob(&j);
    };
  };

}
