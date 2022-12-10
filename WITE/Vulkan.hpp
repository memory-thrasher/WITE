#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#define ALLOCCB (static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks*>(NULL)) //might actually use this later, so pass it everywhere

template<class T> inline constexpr bool bitmaskContains(const vk::Flags<T> l, const vk::Flags<T> r) {
  return (l & r) != vk::Flags<T>(0);
}

template<class T> inline constexpr bool nonzero(const T l) {
  return l != T(0);
};


