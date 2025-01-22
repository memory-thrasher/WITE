/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

int main(int argc, const char** argv) {
  WITE::configuration::setOptions(argc, argv);
  std::filesystem::path path = std::filesystem::temp_directory_path() / "wite_dbindex_test.wdb";
  auto* dbi = new WITE::dbIndex<float>(path, true, WITE::dbIndex<float>::read_cb_F::make([](uint64_t i, float& o) { o = i / 5.0f; }));
  for(uint64_t i = 0;i < 500;i++) {
    dbi->insert(i*3);
    dbi->insert(i*3+1);
  }
  for(uint64_t i = 0;i < 500;i++)
    dbi->remove((i*3+1)/5.0f);
  ASSERT_TRAP(dbi->count() == 500, "got wrong count");
  for(uint64_t i = 0;i < 1600;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < 1500 && i % 3 == 0), "test case failed", i);
  ASSERT_TRAP(dbi->rebalance() == 500, "rebalance got wrong count");
  for(uint64_t i = 0;i < 1600;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < 1500 && i % 3 == 0), "test case failed (after)", i);
};

