/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include "../WITE/WITE.hpp"

/*
  50000 w/ orig
inserts: 111361 (Tests/dbIndex.cpp: 33)
removes: 32634 (Tests/dbIndex.cpp: 38)
count: 1 (Tests/dbIndex.cpp: 46)
pre-rebalance presence check: 81432 (Tests/dbIndex.cpp: 51)
rebalance: 1192584 (Tests/dbIndex.cpp: 58)
post-rebalance presence check: 69 (Tests/dbIndex.cpp: 69)
rebalance 2: 56 (Tests/dbIndex.cpp: 76)
post-rebalance 2 presence check: 66 (Tests/dbIndex.cpp: 87)

5000 w/ orig
inserts: 1081 (Tests/dbIndex.cpp: 47)
removes: 306 (Tests/dbIndex.cpp: 52)
count: 0 (Tests/dbIndex.cpp: 60)
pre-rebalance presence check: 793 (Tests/dbIndex.cpp: 65)
rebalance: 4165 (Tests/dbIndex.cpp: 72)
post-rebalance presence check: 6 (Tests/dbIndex.cpp: 83)
rebalance 2: 2 (Tests/dbIndex.cpp: 90)
post-rebalance 2 presence check: 5 (Tests/dbIndex.cpp: 101)

5000 w/ new rebalance
inserts: 1154 (Tests/dbIndex.cpp: 66)
removes: 322 (Tests/dbIndex.cpp: 71)
count: 0 (Tests/dbIndex.cpp: 79)
pre-rebalance presence check: 842 (Tests/dbIndex.cpp: 84)
rebalance changes made: 22801 (Tests/dbIndex.cpp: 86)
rebalance: 373 (Tests/dbIndex.cpp: 88)
post-rebalance presence check: 5 (Tests/dbIndex.cpp: 98)
rebalance 2 changes made: 0 (Tests/dbIndex.cpp: 100)
rebalance 2: 0 (Tests/dbIndex.cpp: 102)
post-rebalance 2 presence check: 5 (Tests/dbIndex.cpp: 112)

note: new rebalance (above) is flawed, may cause oscillation
5000 2/ rebalance v3 + auto
inserts: 13544 (Tests/dbIndex.cpp: 70)
removes: 10436 (Tests/dbIndex.cpp: 75)
count: 0 (Tests/dbIndex.cpp: 83)
pre-rebalance presence check: 5 (Tests/dbIndex.cpp: 88)
rebalance changes made: 0 (Tests/dbIndex.cpp: 90)
rebalance: 1 (Tests/dbIndex.cpp: 92)
post-rebalance presence check: 5 (Tests/dbIndex.cpp: 102)
rebalance 2 changes made: 0 (Tests/dbIndex.cpp: 104)
rebalance 2: 1 (Tests/dbIndex.cpp: 106)
post-rebalance 2 presence check: 5 (Tests/dbIndex.cpp: 116)

5000 w/ rebalance v3 (no auto)
inserts: 1131 (Tests/dbIndex.cpp: 81)
removes: 312 (Tests/dbIndex.cpp: 86)
count: 0 (Tests/dbIndex.cpp: 94)
pre-rebalance presence check: 817 (Tests/dbIndex.cpp: 99)
rebalance changes made: 23877 (Tests/dbIndex.cpp: 101)
rebalance: 794 (Tests/dbIndex.cpp: 103)
post-rebalance presence check: 5 (Tests/dbIndex.cpp: 113)
rebalance 2 changes made: 0 (Tests/dbIndex.cpp: 115)
rebalance 2: 1 (Tests/dbIndex.cpp: 117)
post-rebalance 2 presence check: 5 (Tests/dbIndex.cpp: 127)

50000 w/ rebalance v3 (no auto)
inserts: 116111 (Tests/dbIndex.cpp: 93)
removes: 33432 (Tests/dbIndex.cpp: 98)
count: 1 (Tests/dbIndex.cpp: 106)
pre-rebalance presence check: 85557 (Tests/dbIndex.cpp: 111)
rebalance changes made: 322334 (Tests/dbIndex.cpp: 113)
rebalance: 80349 (Tests/dbIndex.cpp: 115)
post-rebalance presence check: 64 (Tests/dbIndex.cpp: 125)
rebalance 2 changes made: 0 (Tests/dbIndex.cpp: 127)
rebalance 2: 17 (Tests/dbIndex.cpp: 129)
post-rebalance 2 presence check: 63 (Tests/dbIndex.cpp: 139)


50000 w/ rebalance v3 and manual rebalance every 100 inserts
inserts: 18274 (Tests/dbIndex.cpp: 111)
removes: 218 (Tests/dbIndex.cpp: 116)
count: 0 (Tests/dbIndex.cpp: 124)
pre-rebalance presence check: 502 (Tests/dbIndex.cpp: 129)
rebalance changes made: 45323 (Tests/dbIndex.cpp: 131)
rebalance: 453 (Tests/dbIndex.cpp: 133)
post-rebalance presence check: 65 (Tests/dbIndex.cpp: 143)
rebalance 2 changes made: 499 (Tests/dbIndex.cpp: 145)
rebalance 2: 18 (Tests/dbIndex.cpp: 147)
post-rebalance 2 presence check: 65 (Tests/dbIndex.cpp: 157)

conclusion: ideally rebalance happens after small batches

50000 w/ above + bypassing dbFile locks (dbIndex has its own)
inserts: 9227 (Tests/dbIndex.cpp: 121)
removes: 143 (Tests/dbIndex.cpp: 126)
count: 0 (Tests/dbIndex.cpp: 134)
pre-rebalance presence check: 286 (Tests/dbIndex.cpp: 139)
rebalance changes made: 45323 (Tests/dbIndex.cpp: 141)
rebalance: 252 (Tests/dbIndex.cpp: 143)
post-rebalance presence check: 41 (Tests/dbIndex.cpp: 153)
rebalance 2 changes made: 499 (Tests/dbIndex.cpp: 155)
rebalance 2: 9 (Tests/dbIndex.cpp: 157)
post-rebalance 2 presence check: 41 (Tests/dbIndex.cpp: 167)
*/

constexpr uint64_t testSize = 5000;

uint64_t getNs() {
  return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
};

int main(int argc, const char** argv) {
  WITE::configuration::setOptions(argc, argv);
  std::filesystem::path path = std::filesystem::temp_directory_path() / "wite_dbindex_test.wdb";
  auto* dbi = new WITE::dbIndex<float>(path, true);
  uint64_t lastTime = getNs(), time;
  for(uint64_t i = 0;i < testSize;i++) {
    dbi->insert(i*3, (i*3)/5.0f);
    dbi->insert(i*3, (i*3)/5.0f);
    dbi->insert(i*3+1, (i*3+1)/5.0f);
    if(i%50 == 0)
      dbi->rebalance();
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
  ASSERT_TRAP(temp == testSize*2, "got wrong count: ", temp);
  time = getNs();
  WARN("count specific: ", (time - lastTime)/1000000);
  lastTime = time;
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < testSize*3 && i % 3 == 0), "test case failed", i);
  time = getNs();
  WARN("pre-rebalance presence check: ", (time - lastTime)/1000000);
  lastTime = time;
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP(dbi->count(i / 5.0f) == ((i < testSize*3 && i % 3 == 0) ? 2 : 0), "test case failed", i);
  time = getNs();
  WARN("pre-rebalance count check: ", (time - lastTime)/1000000);
  lastTime = time;
  WARN("rebalance changes made: ", dbi->rebalance());
  time = getNs();
  WARN("rebalance: ", (time - lastTime)/1000000);
  lastTime = time;
#ifdef DEBUG
  temp =
#endif
    dbi->count();
  ASSERT_TRAP(temp == testSize*2, "(post-rebalance count) got wrong count: ", temp);
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < testSize*3 && i % 3 == 0), "test case failed (after)", i);
  time = getNs();
  WARN("post-rebalance presence check: ", (time - lastTime)/1000000);
  lastTime = time;
  WARN("rebalance 2 changes made: ", dbi->rebalance());
  time = getNs();
  WARN("rebalance 2: ", (time - lastTime)/1000000);
  lastTime = time;
#ifdef DEBUG
  temp =
#endif
    dbi->count();
  ASSERT_TRAP(temp == testSize*2, "(post-rebalance count) got wrong count: ", temp);
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP((dbi->findAny(i / 5.0f) == WITE::NONE) ^ (i < testSize*3 && i % 3 == 0), "test case failed (after)", i);
  time = getNs();
  WARN("post-rebalance 2 presence check: ", (time - lastTime)/1000000);
  lastTime = time;
  for(uint64_t i = 0;i < testSize*3 + 100;i++)
    ASSERT_TRAP(dbi->count(i / 5.0f) == ((i < testSize*3 && i % 3 == 0) ? 2 : 0), "test case failed", i);
  time = getNs();
  WARN("post-rebalance 2 count check: ", (time - lastTime)/1000000);
  lastTime = time;
  delete dbi;
};

