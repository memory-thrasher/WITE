#pragma once

#ifdef DEBUG

#include <iostream>

#define ERROR(str) { std::cerr << str; }//TODO set global failure flag that db should read to graceful stop
#define WARN(str) { std::cerr << str; }
#define ASSERT_WARN(cond, msg) { if(cond) { std::cerr << (msg); } }
#define CRASH_PREINIT(msg) { std::cerr << (msg); exit(EXIT_FAILURE); }

#else //release

#define ERROR(str) TODO gui error box
#define WARN(str) TODO log file
#define ASSERT_WARN() {}

#endif
