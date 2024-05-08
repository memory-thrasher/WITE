#pragma once

#define VULKAN_HPP_FLAGS_MASK_TYPE_AS_PUBLIC //makes vk::Flags<T> qualify as "structural" so it can be used in templates
#define VULKAN_HPP_USE_REFLECT // adds reflect() that makes a tuple of any vk struct
#define VULKAN_HPP_STORAGE_SHARED
//vulkan instance and dispatcher storage are provided in gpu.cpp

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define ALLOCCB (static_cast<const VULKAN_HPP_NAMESPACE::AllocationCallbacks*>(NULL)) //might actually use this later, so pass it everywhere

namespace WITE {

  template<class TO, class FROM> consteval TO vkFlagCast(FROM f) {
    return TO(TO::MaskType(FROM::MaskType(f))) & vk::FlagTraits<TO>::allFlags;
  };

  template<typename T, int C> auto& operator<<(std::ostream& s, glm::vec<C, T> v) {
    for(size_t i = 0;i < C;i++)
      s << v[i] << " ";
    return s;
  };

}


