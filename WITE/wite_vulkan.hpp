/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC //makes vk::Flags<T> qualify as "structural" so it can be used in templates
#define VULKAN_HPP_USE_REFLECT // adds reflect() that makes a tuple of any vk struct
//vulkan instance and dispatcher storage are provided in wite_vulkan.cpp

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <iostream>

#define ALLOCCB (static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks*>(NULL)) //might actually use this later, so pass it everywhere

namespace WITE {

  template<class TO, class FROM> consteval TO vkFlagCast(FROM f) {
    return TO(TO::MaskType(FROM::MaskType(f))) & vk::FlagTraits<TO>::allFlags;
  };

  //clang doesn't find these overloads if they're not inside WITE
  template<class T> requires requires(T t) { vk::to_string(t); }//provided by vk for vk enums
  std::ostream& operator<<(std::ostream& s, const T& t) {
    return s << vk::to_string(t);
  };

  std::ostream& operator<<(std::ostream& s, const vk::Extent2D& ex);

}

//clang doesn't find these overloads if they're inside WITE

template<glm::length_t C, typename T, glm::qualifier Q>
std::ostream& operator<<(std::ostream& s, const glm::vec<C, T, Q>& v) {
  for(size_t i = 0;i < C;i++)
    s << v[i] << " ";
  return s;
};

template<glm::length_t R, glm::length_t C, typename T, glm::qualifier Q>
std::ostream& operator<<(std::ostream& s, const glm::mat<R, C, T, Q>& m) {
  for(size_t i = 0;i < C;i++)
    s << "(" << m[i] << ") ";
  return s;
};

