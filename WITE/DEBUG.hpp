/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

//https://stackoverflow.com/questions/6707148/foreach-macro-on-macros-arguments
#define EVAL0(...) __VA_ARGS__
#define EVAL1(...) EVAL0(EVAL0(EVAL0(__VA_ARGS__)))
#define EVAL2(...) EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL3(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL4(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL(...)  EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define MAP_END(...)
#define MAP_OUT
#define MAP_COMMA ,
#define MAP_GET_END2() 0, MAP_END
#define MAP_GET_END1(...) MAP_GET_END2
#define MAP_GET_END(...) MAP_GET_END1
#define MAP_NEXT0(test, next, ...) next MAP_OUT
#define MAP_NEXT1(test, next) MAP_NEXT0(test, next, 0)
#define MAP_NEXT(test, next)  MAP_NEXT1(MAP_GET_END test, next)
#define MAP0(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP1)(f, peek, __VA_ARGS__)
#define MAP1(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP0)(f, peek, __VA_ARGS__)
#define MAP_LIST_NEXT1(test, next) MAP_NEXT0(test, MAP_COMMA next, 0)
#define MAP_LIST_NEXT(test, next)  MAP_LIST_NEXT1(MAP_GET_END test, next)
#define MAP_LIST0(f, x, peek, ...) f(x) MAP_LIST_NEXT(peek, MAP_LIST1)(f, peek, __VA_ARGS__)
#define MAP_LIST1(f, x, peek, ...) f(x) MAP_LIST_NEXT(peek, MAP_LIST0)(f, peek, __VA_ARGS__)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define MAP(f, ...) EVAL(MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define MAP_LIST(f, ...) EVAL(MAP_LIST1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

#ifdef DEBUG

#include <iostream>
#include "syncLock.hpp"
//#include "Thread.hpp"

::WITE::syncLock* LOG_MUTEX();

#define WARN1(msg) { std::cerr << msg; }
#define LOG1(msg) { std::cout << msg; }
// " tid: ", ::WITE::Platform::Thread::getCurrentTid(),
#define WARN(...) { ::WITE::scopeLock lock(LOG_MUTEX()); MAP(WARN1, __VA_ARGS__, " (", std::dec, __FILE__, ": ", __LINE__, ")", std::endl, std::flush); }
#define ERROR(...) { WARN(__VA_ARGS__); asm("INT3"); }//TODO set global failure flag that db should read to graceful stop
#define ASSERT_TRAP(cond, ...) { if(!(cond)) { ERROR(__VA_ARGS__); } }
#define LOG(...) { ::WITE::scopeLock lock(LOG_MUTEX()); MAP(LOG1, __VA_ARGS__, " (", std::dec, __FILE__, ": ", __LINE__, ")", std::endl, std::flush); }
#define DEBUG_TERNARY(debug, release) debug

#else //release

#define ERROR(str) TODO gui error box
#define WARN(str) TODO log file
#define ASSERT_TRAP() {}
#define DEBUG_TERNARY(debug, release) release

#endif

#ifdef WITE_DEBUG_IMAGE_BARRIERS
#define WITE_DEBUG_IB(B, C) { WARN("barrier: command buffer: ", std::hex, C, ", image: ", B.image, std::dec, ", layout: ", (int)B.oldLayout, "->", (int)B.newLayout) }
#define WITE_DEBUG_IBT(B, C, T) { WARN("barrier: command buffer: ", std::hex, C, ", image: ", B.image, std::dec, ", layout: ", (int)B.oldLayout, "->", (int)B.newLayout, " for timing: layerIdx: ", T.layerIdx, ", substep: ", (int)T.substep, ", passId: ", T.passId, ", shaderId: ", T.shaderId) }
#define WITE_DEBUG_IB_CE constexpr
#else
#define WITE_DEBUG_IB(B, C) {}
#define WITE_DEBUG_IBT(B, C, T) {}
#define WITE_DEBUG_IB_CE consteval
#endif

#ifdef WITE_DEBUG_DB
#define WITE_DEBUG_DB_HEADER WARN("dbFile: ", std::hex, this, std::dec, " Header: { freeSpaceLen: ", header->freeSpaceLen, ", allocatedFirst: ", header->allocatedFirst, ", allocatedLast: ", header->allocatedLast, " }")
#define WITE_DEBUG_DB_ALLOCATION(A) WARN("dbFile: ", std::hex, this, std::dec, " Allocation map segment: ", allocatedLEA(A).previous, " <-- ", A, " --> ", allocatedLEA(A).next)
#define WITE_DEBUG_DB_FREESPACE(A) WARN("dbFile: ", std::hex, this, std::dec, " Free space entry at idx ", A, " is now ", freeSpaceLEA(A))
#define WITE_DEBUG_DB_LOG(A) WARN("dbTable: ", std::hex, this, std::dec, " log chain segment: ", logDataFile.deref(A).previousLog, " <-- ", A, " --> ", logDataFile.deref(A).nextLog)
#define WITE_DEBUG_DB_MASTER(A) WARN("dbTable: ", std::hex, this, std::dec, " master row ", A, " firstLog: ", masterDataFile.deref(A).firstLog, " lastLog: ", masterDataFile.deref(A).firstLog)
#else
#define WITE_DEBUG_DB_HEADER {}
#define WITE_DEBUG_DB_ALLOCATION(A) {}
#define WITE_DEBUG_DB_FREESPACE(A) {}
#define WITE_DEBUG_DB_LOG(A) {}
#define WITE_DEBUG_DB_MASTER(A) {}
#endif

#define CRASH_PREINIT(...) { ERROR(__VA_ARGS__); exit(EXIT_FAILURE); }
#define CRASH(...) { ERROR(__VA_ARGS__); } //TODO set a global flag for dbs to graceul down
#define CRASHRET(ret, ...) { CRASH(__VA_ARGS__); return ret; } //return will never happen but it satisfies the compiler
#define CRASHRET_PREINIT(ret, ...) { ERROR(__VA_ARGS__); exit(EXIT_FAILURE); return ret; }

#define VK_ASSERT(cmd, ...) { auto _r = (cmd); if(_r != vk::Result::eSuccess) CRASH(__VA_ARGS__, " ", _r); }
#define VK_ASSERT_TUPLE(out, cmd, ...) { vk::Result _r; std::tie(_r, out) = (cmd); if(_r != vk::Result::eSuccess) CRASH(__VA_ARGS__, _r); }

void constexprAssertFailed();//note: NOT constexpr
#define ASSERT_CONSTEXPR(X) { if(!(X)) ::constexprAssertFailed(); }
//alternate syntax that resembles static_assert
#define constexpr_assert(X) ASSERT_CONSTEXPR(X)
#define ASSERT_CONSTEXPR_RET(X, RET) { if(!(X)) { ::constexprAssertFailed(); return RET; } }

template<bool condition, class T, T values> struct static_assert_show_t {
  static_assert(condition);
  constexpr ~static_assert_show_t() {};//non-default destructor suppresses unused variable warning
};
#define static_assert_show(C, V) constexpr static static_assert_show_t<C, decltype(V), V> sast ## __LINE__;

template<class T> void hexdump(T* src) {
  uint8_t* data = reinterpret_cast<uint8_t*>(src);
  for(size_t i = 0;i < sizeof(T);i++)
    std::cout << std::hex << static_cast<int>(data[i]) << " ";
  std::cout << std::endl;
}

