#pragma once

#include <time.h>
#include <stdint.h>

namespace WITE::Util {

  timespec now();
  timespec since(const timespec&);
  timespec diff(const timespec&, const timespec&);
  uint64_t toNS(const timespec&);

}
