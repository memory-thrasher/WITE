/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <atomic>
#include <mutex> //not using SyncLock because we want to profile it too
#include <map>

#ifdef DO_PROFILE
#define PROFILEME ::WITE::profiler UNIQUENAME(wite_function_profiler) (::WITE::profiler::hash(__FILE__, __func__, __LINE__, ""), __FILE__, __func__, __LINE__, "")
#define PROFILEME_MSG(MSG) ::WITE::profiler UNIQUENAME(wite_function_profiler) (::WITE::profiler::hash(__FILE__, __func__, __LINE__, MSG), __FILE__, __func__, __LINE__, MSG)
#define PROFILE_DUMP ::WITE::profiler::printProfileData();
#else
#define PROFILEME
#define PROFILEME_MSG(MSG)
#define PROFILE_DUMP
#endif

namespace WITE {

  class profiler {
  private:
    typedef uint64_t hash_t;
    struct ProfileData {
      std::atomic_uint64_t executions, totalTimeNs, min = ~0, max;
      char identifier[4096];
    };

    static std::map<hash_t, ProfileData> allProfiles;
    static std::mutex allProfiles_mutex;
    static std::atomic_uint64_t allProfilesMutexTime, allProfilesExecutions;
    static uint64_t getNs();
    char identifier[4096];
    uint64_t startTime;
    hash_t h;

  public:
    static constexpr hash_t hash(const char* filename, const char* funcname, uint64_t linenum, const char* message) {
      hash_t hash = linenum * 101;
      for(size_t i = 0;filename[i];i++)
	hash = hash * 257 + filename[i];
      hash *= 313;
      for(size_t i = 0;funcname[i];i++)
	hash = hash * 257 + funcname[i];
      hash *= 433;
      for(size_t i = 0;message[i];i++)
	hash = hash * 257 + message[i];
      return hash;
    };

    static void printProfileData();

    profiler(hash_t hash, const char* filename, const char* funcname, uint64_t linenum, const char* message);//hash is split off so it can be constexpr
    ~profiler();

  };

}
