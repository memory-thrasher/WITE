#pragma once

namespace WITE {

  class export_def Time {
  public:
    static uint64_t frame();
    static uint64_t delta();
    static uint64_t lastFrame();
    static uint64_t launchTime();
    static uint64_t nowNs();
    static uint64_t nowMs();
  private:
    Time() = delete;
  };

}
