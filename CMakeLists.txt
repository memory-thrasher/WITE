cmake_minimum_required(VERSION 2.8.11)
project(TestCube)
set(COMMON_COMPILE_FLAGS "-Wall -Wextra -Werror")
set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -iquote /usr/include/SDL2")
#TODO more robust library detection; see https://stackoverflow.com/questions/68170919/cmake-find-system-include-directory-not-by-name-or-content-c-c-relative
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c++17")
add_subdirectory(WaffleIronTransactionalEngine)
