#pragma once

#include <array>

#include "Database.hpp"

namespace WITE::Transient {

  template<class T, size_t N = 2> class FrameSwappedResource {
  private:
    DB::Database* db;
    std::array<T, N> data;
    static_assert(N > 0);
  public:
    FrameSwappedResource(DB::Database* db, const std::array<T, N>& data) : db(db), data(data) {}
    FrameSwappedResource(FrameSwappedResource) = delete;
    T get(ssize_t offset = 0) {
      size_t idx = db->getFrame() + offset;
      if(idx < 0) [[unlikely]] idx = N + idx % N;
      else if(idx > (N << 1)) [[unlikely]] idx = idx % N;
      else if(idx > N) [[unlikely]] idx -= N;
      return data[idx];
    }
  };

}
