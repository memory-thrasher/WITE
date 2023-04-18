#ifdef DEBUG

#include "DEBUG.hpp"

::WITE::Util::SyncLock logMutex;
::WITE::Util::SyncLock* LOG_MUTEX() { return &logMutex; };

int constexprAssertFailsRuntime = 0;
void constexprAssertFailed() { constexprAssertFailsRuntime++; };

#endif
