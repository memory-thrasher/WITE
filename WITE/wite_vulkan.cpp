#include "wite_vulkan.hpp"

namespace WITE {
  
  std::ostream& operator<<(std::ostream& s, vk::Result r) {
    return s << vk::to_string(r);
  };

};
