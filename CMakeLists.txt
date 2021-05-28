cmake_minimum_required(VERSION 2.8.11)
project(TestCube)
# set (CMAKE_VERBOSE_MAKEFILE 1)
set(API_NAME "Vulkan" CACHE STRING "API name to use when building")
string(TOLOWER ${API_NAME} API_LOWERCASE)
include(GNUInstallDirs)
file(TO_CMAKE_PATH $ENV{VULKAN_SDK} VULKAN)

# The MAJOR number of the version we're building, used in naming
# vulkan-<major>.dll (and other files).
set(MAJOR "1")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set (CMAKE_INSTALL_PREFIX "")
if(NOT WIN32)
    include(FindPkgConfig)
    option(BUILD_WSI_XCB_SUPPORT "Build XCB WSI support" ON)
    option(BUILD_WSI_WAYLAND_SUPPORT "Build Wayland WSI support" OFF)
    set(DEMOS_WSI_SELECTION "XCB" CACHE STRING "Select WSI target for demos (XCB, XLIB, WAYLAND, DISPLAY)")
    set(SAMPLES_WSI_SELECTION "XCB" CACHE STRING "Select WSI target for api-samples (XCB, WAYLAND, DISPLAY)")
    if (BUILD_WSI_XCB_SUPPORT)
         find_package(XCB REQUIRED)
    endif()
    if (BUILD_WSI_WAYLAND_SUPPORT)
        find_package(Wayland REQUIRED)
    endif()
    set (BUILDTGT_DIR build)
    set (BINDATA_DIR x86_64/bin)
    set (LIBSOURCE_DIR Lib)
else()
    # For Windows, since 32-bit and 64-bit items can co-exist, we build each in its own build directory.
    # 32-bit target data goes in build32, and 64-bit target data goes into build.  So, include/link the
    # appropriate data at build time.
    if (CMAKE_CL_64)
        set (BUILDTGT_DIR build)
        set (BINDATA_DIR Bin)
        set (LIBSOURCE_DIR Lib)
    else()
        set (BUILDTGT_DIR build32)
        set (BINDATA_DIR Bin32)
        set (LIBSOURCE_DIR Lib32)
    endif()
endif()

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(COMMON_COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -fno-strict-aliasing -fno-builtin-memcmp")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_COMPILE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c++17")
    if (UNIX)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    endif()
endif()
find_program(GLSLANG_VALIDATOR NAMES glslangValidator
             HINTS "${VULKAN}/glslang/${BUILDTGT_DIR}/install/bin"
                   "${VULKAN}/${BINDATA_DIR}" )
find_path(GLSLANG_SPIRV_INCLUDE_DIR SPIRV/spirv.hpp HINTS "${VULKAN}/source/glslang/"
                                                          "${VULKAN}/glslang"
                                                          DOC "Path to SPIRV/spirv.hpp")
find_path(SPIRV_TOOLS_INCLUDE_DIR spirv-tools/libspirv.h HINTS "${VULKAN}/spirv-tools/include"
                                                               "${VULKAN}/source/spirv-tools/include"
                                                               "${VULKAN}/spirv-tools/external/include"
                                                               "${VULKAN}/source/spirv-tools/external/include"
                                                               DOC "Path to spirv-tools/libspirv.h")
find_path(Vulkan_INCLUDE_DIR vulkan/vulkan.h HINTS	"${VULKAN}"
													"${VULKAN}/Include" )

find_path(glm_INCLUDE_DIR glm/glm.hpp HINTS "${VULKAN}/Third-Party/Include"
											"${VULKAN}/Third-Party"
											"${VULKAN}"
											"${VULKAN}/Include" )

if (WIN32)
    set (GLSLANG_SEARCH_PATH "${VULKAN}/glslang/${BUILDTGT_DIR}/glslang/Release"
                             "${VULKAN}/glslang/${BUILDTGT_DIR}/glslang/OSDependent/Windows/Release"
                             "${VULKAN}/glslang/${BUILDTGT_DIR}/hlsl/Release"
                             "${VULKAN}/glslang/${BUILDTGT_DIR}/OGLCompilersDLL/Release"
                             "${VULKAN}/glslang/${BUILDTGT_DIR}/SPIRV/Release" )
    set (SPIRV_TOOLS_SEARCH_PATH "${VULKAN}/spirv-tools/${BUILDTGT_DIR}/source/Release")
    set (SPIRV_TOOLS_OPT_SEARCH_PATH "${VULKAN}/spirv-tools/${BUILDTGT_DIR}/source/opt/Release")
else()
    set (GLSLANG_SEARCH_PATH "${VULKAN}/glslang/build/install/lib" "${VULKAN}/x86_64/lib/glslang" )
    set (SPIRV_TOOLS_SEARCH_PATH "${VULKAN}/spirv-tools/build" "${VULKAN}/x86_64/lib/spirv-tools" )
    set (SPIRV_TOOLS_OPT_SEARCH_PATH "${SPIRV_TOOLS_SEARCH_PATH}")
endif()

find_library(GLSLANG_LIB NAMES glslang
             HINTS ${GLSLANG_SEARCH_PATH} )

find_library(OGLCompiler_LIB NAMES OGLCompiler
             HINTS ${GLSLANG_SEARCH_PATH} )

find_library(OSDependent_LIB NAMES OSDependent
             HINTS ${GLSLANG_SEARCH_PATH} )

find_library(HLSL_LIB NAMES HLSL
             HINTS ${GLSLANG_SEARCH_PATH} )

find_library(SPIRV_LIB NAMES SPIRV
             HINTS ${GLSLANG_SEARCH_PATH} )

find_library(SPIRV_TOOLS_LIB NAMES SPIRV-Tools
             HINTS ${SPIRV_TOOLS_SEARCH_PATH} )

find_library(SPIRV_TOOLS_OPT_LIB NAMES SPIRV-Tools-opt
             HINTS ${SPIRV_TOOLS_OPT_SEARCH_PATH} )

find_library(SPIRV_REMAPPER_LIB NAMES SPVRemapper
             HINTS ${GLSLANG_SEARCH_PATH} )

# On Windows, we must pair Debug and Release appropriately
if (WIN32)
    set (GLSLANG_DEBUG_SEARCH_PATH "${VULKAN}/glslang/${BUILDTGT_DIR}/glslang/Debug"
                                   "${VULKAN}/glslang/${BUILDTGT_DIR}/glslang/OSDependent/Windows/Debug"
                                   "${VULKAN}/glslang/${BUILDTGT_DIR}/hlsl/Debug"
                                   "${VULKAN}/glslang/${BUILDTGT_DIR}/OGLCompilersDLL/Debug"
                                   "${VULKAN}/glslang/${BUILDTGT_DIR}/SPIRV/Debug")
    set (SPIRV_TOOLS_DEBUG_SEARCH_PATH "${VULKAN}/spirv-tools/${BUILDTGT_DIR}/source/Debug")
    set (SPIRV_TOOLS_OPT_DEBUG_SEARCH_PATH "${VULKAN}/spirv-tools/${BUILDTGT_DIR}/source/opt/Debug")

    add_library(glslang     STATIC IMPORTED)
    add_library(OGLCompiler STATIC IMPORTED)
    add_library(OSDependent STATIC IMPORTED)
    add_library(HLSL        STATIC IMPORTED)
    add_library(SPIRV       STATIC IMPORTED)
    add_library(Loader      STATIC IMPORTED)
    add_library(SPIRV-Tools STATIC IMPORTED)
    add_library(SPIRV-Tools-opt STATIC IMPORTED)
    add_library(SPVRemapper STATIC IMPORTED)

    find_library(GLSLANG_DLIB NAMES glslangd
                 HINTS ${GLSLANG_DEBUG_SEARCH_PATH} )
    find_library(OGLCompiler_DLIB NAMES OGLCompilerd
                 HINTS ${GLSLANG_DEBUG_SEARCH_PATH} )
    find_library(OSDependent_DLIB NAMES OSDependentd
                 HINTS ${GLSLANG_DEBUG_SEARCH_PATH} )
    find_library(HLSL_DLIB NAMES HLSLd
                 HINTS ${GLSLANG_DEBUG_SEARCH_PATH} )
    find_library(SPIRV_DLIB NAMES SPIRVd
                 HINTS ${GLSLANG_DEBUG_SEARCH_PATH} )
    find_library(SPIRV_TOOLS_DLIB NAMES SPIRV-Tools
                 HINTS ${SPIRV_TOOLS_DEBUG_SEARCH_PATH} )
    find_library(SPIRV_TOOLS_OPT_DLIB NAMES SPIRV-Tools-opt
                 HINTS ${SPIRV_TOOLS_OPT_DEBUG_SEARCH_PATH} )
    find_library(SPIRV_REMAPPER_DLIB NAMES SPVRemapperd
                 HINTS ${GLSLANG_DEBUG_SEARCH_PATH} )

    set_target_properties(glslang PROPERTIES
                         IMPORTED_LOCATION       "${GLSLANG_LIB}"
                         IMPORTED_LOCATION_DEBUG "${GLSLANG_DLIB}")
    set_target_properties(OGLCompiler PROPERTIES
                         IMPORTED_LOCATION       "${OGLCompiler_LIB}"
                         IMPORTED_LOCATION_DEBUG "${OGLCompiler_DLIB}")
    set_target_properties(OSDependent PROPERTIES
                         IMPORTED_LOCATION       "${OSDependent_LIB}"
                         IMPORTED_LOCATION_DEBUG "${OSDependent_DLIB}")
    set_target_properties(HLSL PROPERTIES
                         IMPORTED_LOCATION       "${HLSL_LIB}"
                         IMPORTED_LOCATION_DEBUG "${HLSL_DLIB}")
    set_target_properties(SPIRV PROPERTIES
                         IMPORTED_LOCATION       "${SPIRV_LIB}"
                         IMPORTED_LOCATION_DEBUG "${SPIRV_DLIB}")
    set_target_properties(SPIRV-Tools PROPERTIES
                         IMPORTED_LOCATION       "${SPIRV_TOOLS_LIB}"
                         IMPORTED_LOCATION_DEBUG "${SPIRV_TOOLS_DLIB}")
    set_target_properties(SPIRV-Tools-opt PROPERTIES
                         IMPORTED_LOCATION       "${SPIRV_TOOLS_OPT_LIB}"
                         IMPORTED_LOCATION_DEBUG "${SPIRV_TOOLS_OPT_DLIB}")
    set_target_properties(SPVRemapper PROPERTIES
                         IMPORTED_LOCATION       "${SPIRV_REMAPPER_LIB}"
                         IMPORTED_LOCATION_DEBUG "${SPIRV_REMAPPER_DLIB}")

    set (SPIRV_TOOLS_LIBRARIES SPIRV-Tools-opt SPIRV-Tools)
    set (GLSLANG_LIBRARIES glslang OGLCompiler OSDependent HLSL SPIRV SPVRemapper ${SPIRV_TOOLS_LIBRARIES})
else ()
    set (SPIRV_TOOLS_LIBRARIES ${SPIRV_TOOLS_OPT_LIB} ${SPIRV_TOOLS_LIB})
    set (GLSLANG_LIBRARIES ${GLSLANG_LIB} ${OGLCompiler_LIB} ${OSDependent_LIB} ${HLSL_LIB} ${SPIRV_LIB} ${SPIRV_REMAPPER_LIB} ${SPIRV_TOOLS_LIBRARIES})
endif()

# to run with source glslang libs/headers
if(UNIX)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    if (EXISTS "${VULKAN}/glslang")
         set (GLSLANGDIR "${VULKAN}/glslang")
    else()
         set (GLSLANGDIR "${VULKAN}/source/glslang")
    endif()
else()
    set (GLSLANGDIR "${VULKAN}/glslang")
endif()

get_filename_component(GLSLANG_PREFIX "${GLSLANGDIR}" ABSOLUTE)
if(NOT EXISTS ${GLSLANG_PREFIX})
    message(FATAL_ERROR "Necessary glslang components do not exist: " ${GLSLANG_PREFIX})
endif()

if(WIN32)
    set (MOVE_CMD "move")
    set (VULKAN_LOADER_NAME "${API_LOWERCASE}-${MAJOR}")
else()
    set (MOVE_CMD "mv")
    set (PTHREAD "pthread")
    set (VULKAN_LOADER_NAME "${API_LOWERCASE}")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/layers")
    set (VULKAN_LOADER ${VULKAN_LOADER_NAME})
else()
    find_library(VULKAN_LOADER NAMES ${VULKAN_LOADER_NAME}
                 HINTS "${VULKAN}/${LIBSOURCE_DIR}" "${VULKAN}/x86_64/lib" )
endif()

add_definitions(-DAPI_NAME="${API_NAME}")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR -DWIN32_LEAN_AND_MEAN)
    set(DisplayServer Win32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (SAMPLES_WSI_SELECTION STREQUAL "XCB")
        if (NOT BUILD_WSI_XCB_SUPPORT)
            message( FATAL_ERROR "Selected XCB for samples build but not building Xcb support" )
        endif()
        add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    elseif (SAMPLES_WSI_SELECTION STREQUAL "XLIB")
        if (NOT BUILD_WSI_XLIB_SUPPORT)
            message( FATAL_ERROR "Selected XLIB for samples build but not building Xlib support" )
        endif()
        add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
    elseif (SAMPLES_WSI_SELECTION STREQUAL "WAYLAND")
        if (NOT BUILD_WSI_WAYLAND_SUPPORT)
            message( FATAL_ERROR "Selected Wayland for samples build but not building Wayland support" )
        endif()
        add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR)
    endif()
else()
    message(FATAL_ERROR "Unsupported Platform!")
endif()

include_directories( ${GLSLANG_SPIRV_INCLUDE_DIR} ${GLMINC_PREFIX} ${Vulkan_INCLUDE_DIR})

# Additional includes for spirv-tools
include_directories(${SPIRV_TOOLS_INCLUDE_DIR})

if(EXISTS "${PROJECT_SOURCE_DIR}/${V_LVL_RELATIVE_LOCATION}/loader")
    set (VULKAN_LOADER ${VULKAN_LOADER_NAME})
else()
    find_library(VULKAN_LOADER NAMES ${VULKAN_LOADER_NAME}
    HINTS "${V_LVL_RELATIVE_LOCATION}/${LIBSOURCE_DIR}" "${V_LVL_RELATIVE_LOCATION}/x86_64/lib" )
endif()

set (LIBGLM_INCLUDE_DIR ${V_LVL_RELATIVE_LOCATION}/libs)

if(WIN32)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES")
    if (MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4267")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267")
    endif()
else()
    if(SDK_INCLUDE_PATH)
        include_directories(${SDK_INCLUDE_PATH})
    endif()
    if (BUILD_WSI_XCB_SUPPORT)
        include_directories(${XCB_INCLUDE_DIRS})
        link_libraries(${XCB_LIBRARIES} m )
    endif()
    if (BUILD_WSI_WAYLAND_SUPPORT)
        include_directories(${WAYLAND_CLIENT_INCLUDE_DIR})
        link_libraries(${WAYLAND_CLIENT_LIBRARIES})
    endif()
    link_libraries(${VULKAN_LOADER} m ) 
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
endif()

if("${STANDALONE}")
	add_definitions("-DSTANDALONE")
endif()
option(ANDROID OFF)

include_directories(${glm_INCLUDE_DIR})

add_subdirectory("WaffleIronTransactionalEngine")
