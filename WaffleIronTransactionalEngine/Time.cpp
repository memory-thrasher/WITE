#include "Internal.h"

static uint64_t lTime, fTime, pTime, dTime;

namespace WITE {

  export_def uint64_t Time::frame() {
    return fTime;
  }

  export_def uint64_t Time::delta() {
    return dTime;
  }

  export_def uint64_t Time::lastFrame() {
    return pTime;
  }

  export_def uint64_t Time::launchTime() {
    return lTime;
  }

  export_def uint64_t Time::nowNs() {
    return timeNs();
  }

  export_def uint64_t Time::nowMs() {
    return timeNs() / 1000000ull;
  }

}

void updateTime() {
  pTime = fTime;
  fTime = WITE::Time::nowNs();
  dTime = fTime - pTime;
}

void initTime() {
  lTime = fTime = pTime = WITE::Time::nowNs();
  dTime = 1;//0 might break an fps counter
}
