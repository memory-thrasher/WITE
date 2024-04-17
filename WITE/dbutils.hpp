#pragma once

#include <type_traits>

#include "dbTemplateStructs.hpp"
#include "threadPool.hpp"

namespace WITE {

  template<class T> struct has_update : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { T::update(oid) -> void } struct has_update : public std::true_type {};

  template<class T> struct has_allocated : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { T::allocated(oid) -> void } struct has_allocated : public std::true_type {};

  template<class T> struct has_freed : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { T::freed(oid) -> void } struct has_freed : public std::true_type {};

  template<class T> struct has_spunUp : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { T::spunUp(oid) -> void } struct has_spunUp : public std::true_type {};

  template<class T> struct has_spunDown : public std::false_type {};
  template<class T> requires requires(uint64_t oid) { T::spunDown(oid) -> void } struct has_spunDown : public std::true_type {};

  //so we don't have to malloc up a new callbackPtr for every object being updated, reuse the callback object and store the oid in jobData
  template<class T, void(*F)(uint64_t)> struct dbJobWrapper {
    static constexpr threadPool::jobEntry_t_F::StaticCallback<> cbt = F;
    static constexpr threadPool::jobEntry_t_ce cbce = &cbt;
    threadPool::job_t j;
    dbJobWrapper(uint64_t oid, threadPool& tp) : j({}, cbce) {
      tp.submitJob(&j);
    };
  };

}
