#ifdef DEBUG

#include "DEBUG.hpp"

::WITE::syncLock logMutex;
::WITE::syncLock* LOG_MUTEX() { return &logMutex; };

int constexprAssertFailsRuntime = 0;
void constexprAssertFailed() { constexprAssertFailsRuntime++; };

#endif
