/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

constexpr uint64_t testSize = 50000;

uint64_t getNs() {
  return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
};

int main(int argc, const char** argv) {
  WITE::configuration::setOptions(argc, argv);
  std::filesystem::path path = std::filesystem::temp_directory_path() / "wite_dbindex_test.wdb";
  auto* dbi = new WITE::dbIndex<float>(path, true, WITE::dbIndex<float>::read_cb_F::make([](uint64_t i, float& o) { o = i / 5.0f; }));
  uint64_t lastTime = getNs(), time;
  for(uint64_t i = 0;i < testSize;i++) {
    dbi->insert(i*3);
    dbi->insert(i*3+1);
  }
  time = getNs();
  WARN("inserts: ", (time - lastTime)/1000000);
  lastTime = time;
  for(uint64_t i = 0;i < testSize;i++)
    dbi->remove((i*3+1)/5.0f);
  time = getNs();
  WARN("removes: ", (time - lastTime)/1000000);
  lastTime = time;
#ifdef DEBUG
  uint64_t temp =
#endif
    dbi->count();
  ASSERT_TRAP(temp == testSize, "got wrong count: ", temp);
  time = getNs();
  WARN("count: ", (time - lastTime)/1000000);
  lastTime = time;
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < testSize*3 && i % 3 == 0), "test case failed", i);
  time = getNs();
  WARN("pre-rebalance presence check: ", (time - lastTime)/1000000);
  lastTime = time;
#ifdef DEBUG
  temp =
#endif
    dbi->rebalance();
  time = getNs();
  WARN("rebalance: ", (time - lastTime)/1000000);
  lastTime = time;
  ASSERT_TRAP(temp == testSize, "rebalance got wrong count: ", temp);
#ifdef DEBUG
  temp =
#endif
    dbi->count();
  ASSERT_TRAP(temp == testSize, "(post-rebalance count) got wrong count: ", temp);
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < testSize*3 && i % 3 == 0), "test case failed (after)", i);
  time = getNs();
  WARN("post-rebalance presence check: ", (time - lastTime)/1000000);
  lastTime = time;
#ifdef DEBUG
  temp =
#endif
    dbi->rebalance();
  time = getNs();
  WARN("rebalance 2: ", (time - lastTime)/1000000);
  lastTime = time;
  ASSERT_TRAP(temp == testSize, "rebalance got wrong count: ", temp);
#ifdef DEBUG
  temp =
#endif
    dbi->count();
  ASSERT_TRAP(temp == testSize, "(post-rebalance count) got wrong count: ", temp);
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < testSize*3 && i % 3 == 0), "test case failed (after)", i);
  time = getNs();
  WARN("post-rebalance 2 presence check: ", (time - lastTime)/1000000);
  lastTime = time;
};

