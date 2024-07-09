#define VULKAN_HPP_STORAGE_SHARED_EXPORT

#include "wite_vulkan.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace WITE {
  
  std::ostream& operator<<(std::ostream& s, vk::Result r) {
    return s << vk::to_string(r);
  };

};
