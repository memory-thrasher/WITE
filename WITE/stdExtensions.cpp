#include "stdExtensions.hpp"
#include "DEBUG.hpp"

namespace WITE {

  std::string concat(const std::initializer_list<const std::string> strings) {
    std::string ret;
    size_t totalLen = 0;
    for(const std::string& s : strings)
      totalLen += s.size();
    ret.reserve(totalLen);
    for(const std::string& s : strings)
      ret.append(s);
    return ret;
  }

}
