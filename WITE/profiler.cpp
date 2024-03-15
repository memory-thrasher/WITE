#include <assert.h>
#include <string.h>

#include "profiler.hpp"
#include "DEBUG.hpp"

namespace WITE {

  //statics
  std::map<profiler::hash_t, profiler::ProfileData> profiler::allProfiles;
  std::mutex profiler::allProfiles_mutex;
  std::atomic_uint64_t profiler::allProfilesMutexTime;
  std::atomic_uint64_t profiler::allProfilesExecutions;
  timer_t profiler::timer;
  bool profiler::initDone = false;

  uint64_t profiler::getNs() { //static
    static constexpr struct itimerspec MAX_TIME { { 0, 0 }, { 60*60*24*365*16, 0 } };
    if(!initDone) {
      std::lock_guard<std::mutex> lock(allProfiles_mutex);
      if(!initDone) {
	struct sigevent noop;
	noop.sigev_notify = SIGEV_NONE;
	assert(!timer_create(CLOCK_REALTIME, &noop, &timer));
	assert(!timer_settime(timer, 0, &MAX_TIME, NULL));
	initDone = true;
	return 0;
      }
    }
    struct itimerspec time;
    assert(!timer_gettime(timer, &time));
    return (MAX_TIME.it_value.tv_sec - time.it_value.tv_sec) * 1000000000 +
      (MAX_TIME.it_value.tv_nsec - time.it_value.tv_nsec);
  };

  void profiler::printProfileData() { //static
    std::lock_guard<std::mutex> lock(allProfiles_mutex);
    for(auto& pair : allProfiles) {
      printf("%100s:\t\ttotal: %15lu\texecutions: %15lu\taverage: %15lu\n",
	     pair.second.identifier,
	     pair.second.totalTimeNs.load(),
	     pair.second.executions.load(),
	     pair.second.totalTimeNs.load() / pair.second.executions.load());
    }
    printf("%100s:\t\ttotal: %15lu\texecutions: %15lu\taverage: %15lu\n",
	   "Profiling overhead",
	   allProfilesMutexTime.load(),
	   allProfilesExecutions.load(),
	   allProfilesMutexTime.load() / allProfilesExecutions.load());
  };

  profiler::profiler(hash_t hash, const char* filename, const char* funcname, uint64_t linenum, const char* message) :
    h(hash)
  {
    // int len = sprintf(NULL, "%s::%s:%lu (%s)", filename, funcname, linenum, message);
    // ASSERT_TRAP(len > 0 && len < sizeof(identifier), "profiler identifier generation failure");
    sprintf(identifier, "%s::%s:%lu (%s)", filename, funcname, linenum, message);
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
    pd->executions++;
    pd->totalTimeNs += endTime - startTime;
    allProfilesExecutions++;
    postEnd = getNs();
    allProfilesMutexTime += postEnd - endTime;
  };

}

