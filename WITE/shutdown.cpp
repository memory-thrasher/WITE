#include <atomic>

#include "shutdown.hpp"

namespace WITE {
  std::atomic_bool shuttingDown = false;
  bool shutdownRequested() {
    return shuttingDown;
  };
  void requestShutdown() {
    shuttingDown = true;
  };
}
