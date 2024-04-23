#pragma once

//if you need an include, it doesn't belong here
//defines wrapped in ifndef mean they can be overriden at compile-time i.e. -D. Both the shared lib and any application must have the same override(s)!

#ifndef MAX_GPUS
#define MAX_GPUS 8
#endif

#ifndef MIN_LOG_HISTORY
#define MIN_LOG_HISTORY 3
#endif

#define CONCAT1(a, b) a ## b
#define CONCAT(a, b) CONCAT1(a, b)
#define UNIQUENAME(prefix) CONCAT(prefix, __LINE__)

namespace WITE {

};
