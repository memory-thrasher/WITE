/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#include <memory>
#include <chrono>
#include <iostream>

#include "profiler.hpp"
#include "DEBUG.hpp"
#include "stdExtensions.hpp"

namespace WITE {

  //statics
  std::map<profiler::hash_t, profiler::ProfileData> profiler::allProfiles;
  std::mutex profiler::allProfiles_mutex;
  std::atomic_uint64_t profiler::allProfilesMutexTime;
  std::atomic_uint64_t profiler::allProfilesExecutions;

  uint64_t profiler::getNs() { //static
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
  };

  void profiler::printProfileData() { //static
    std::unique_ptr<ProfileData*[]> data;
    size_t cnt;
    auto totalExecutions = allProfilesExecutions.load();
    if(totalExecutions == 0) {
      printf("No profile data");
      return;
    }
    {
      std::lock_guard<std::mutex> lock(allProfiles_mutex);
      cnt = allProfiles.size();
      data = std::make_unique<ProfileData*[]>(cnt);
      size_t i = 0;
      for(auto& pair : allProfiles)
	data[i++] = &pair.second;
    }
    std::sort(&data[0], &data[cnt], [](const auto a, const auto b) { return a->totalTimeNs < b->totalTimeNs; });
    for(size_t i = 0;i < cnt;i++) {
      auto* datum = data[i];
      std::cout << datum->identifier << ": \ttotal: " << datum->totalTimeNs.load() <<
	" \texecutions: " << datum->executions.load() <<
	" \taverage: " << (datum->totalTimeNs.load() / datum->executions.load()) <<
	" \tmin: " << datum->min.load() <<
	" \tmax: " << datum->max.load() << "\n";
    }
    std::cout << "Profiling overhead: \ttotal: " << allProfilesMutexTime.load() <<
      " \texecutions: " << allProfilesExecutions.load() <<
      " \taverage: " << (allProfilesMutexTime.load() / totalExecutions) << "\n";
  };

  profiler::profiler(hash_t hash, const char* filename, const char* funcname, uint64_t linenum, const char* message) :
    h(hash)
  {
    // int len = sprintf(NULL, "%s::%s:%lu (%s)", filename, funcname, linenum, message);
    // ASSERT_TRAP(len > 0 && len < sizeof(identifier), "profiler identifier generation failure");
    sprintf(identifier, "%s::%s:%llu (%s)", filename, funcname, (long long unsigned int)linenum, message);
    startTime = getNs();
  };

  profiler::~profiler() {
    uint64_t endTime = getNs(), postEnd;
    ProfileData* pd;
    {
      std::lock_guard<std::mutex> lock(allProfiles_mutex);
      pd = &allProfiles[h];
      if(strlen(pd->identifier) == 0)
	strcpy(pd->identifier, identifier);
    }
    auto time = endTime - startTime;
    pd->executions++;
    pd->totalTimeNs += time;
    atomicMin(pd->min, time);
    atomicMax(pd->max, time);
    allProfilesExecutions++;
    postEnd = getNs();
    allProfilesMutexTime += postEnd - endTime;
  };

}

