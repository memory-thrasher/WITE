#pragma once

#include <atomic>
#include <mutex> //not using SyncLock because we want to profile it too
#include <map>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

#ifdef DO_PROFILE
#define PROFILEME ::WITE::profiler UNIQUENAME(wite_function_profiler) (::WITE::profiler::hash(__FILE__, __func__, __LINE__, ""), __FILE__, __func__, __LINE__, "")
#define PROFILEME_MSG(MSG) ::WITE::profiler UNIQUENAME(wite_function_profiler) (::WITE::profiler::hash(__FILE__, __func__, __LINE__, MSG), __FILE__, __func__, __LINE__, MSG)
#else
#define PROFILEME
#define PROFILEME_MSG
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
    static timer_t timer;
    static bool initDone;
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
