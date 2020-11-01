#pragma once

namespace WITE {

  class export_def GPU {//these are created by init and may be present as args in callbacks
  public:
    ~GPU() = default;
    GPU(const GPU&) = delete;
  protected:
    GPU() = default;
  };

}
