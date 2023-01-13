#pragma once

//use the PLATFORM macro if you have to test for platform elsewhere, but in theory it's mostly in this file
#if defined(WIN32)
#error NYI
#define PLATFORM WIN32
#include "PlatformSpecific_WIN32.hpp"
//maybe a special case may be needed for cygwin if someone wants to run a game engine on cygwin
#elif defined(__linux__)
#define PLATFORM LINUX
#else
#error failed to identify platform
#end

#define PLATFORM_HEADER "PlatformSpecific_ ##PLATFORM## .hpp"
#include PLATFORM_HEADER
#undefine PLATFORM_HEADER

