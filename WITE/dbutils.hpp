#pragma once

#include <type_traits>

#include "dbTemplateStructs.hpp"

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


}
