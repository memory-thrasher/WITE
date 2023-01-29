#pragma once

//if you need an include, it doesn't belong here
//defines wrapped in ifndef mean they can be overriden at compile-time i.e. -D. Both the shared lib and any application must have the same override(s)!

#ifndef MAX_GPUS
#define MAX_GPUS 4
#endif

#ifndef MAX_LDMS
#define MAX_LDMS 8
#endif

#ifndef DB_THREAD_COUNT
#define DB_THREAD_COUNT 16
#endif

#ifndef PROMISE_THREAD_COUNT
#define PROMISE_THREAD_COUNT (DB_THREAD_COUNT/2)
#endif

#ifndef PROMISE_THREAD_COUNT
#define PROMISE_THREAD_COUNT (DB_THREAD_COUNT/2)
#endif

#ifndef MAX_THREADS
#define MAX_THREADS 256
#endif

#define GENERATED_THREAD_COUNT (PROMISE_THREAD_COUNT + DB_THREAD_COUNT)

static_assert(GENERATED_THREAD_COUNT + 1 < MAX_THREADS);//+1 for main

namespace WITE {

};
