cmake_minimum_required (VERSION 2.8.11)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()

# Fix a warning on Mac OS X.
if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif ()

# Don't use -rdynamic since it breaks causes musl static linking.
if (POLICY CMP0065)
  cmake_policy(SET CMP0065 NEW)
endif ()

project (p-load)

find_package(PkgConfig)

# Make 'Release' be the default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Options are Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif ()

set(USE_SYSTEM_TINYXML2 FALSE CACHE BOOL
  "True if you want to use TinyXML-2 from the system instead of the bundled one.")

# Our C++ code uses features from the C++11 standard.
macro(use_cxx11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_CXX_FLAGS "-std=gnu++0x ${CMAKE_CXX_FLAGS}")
    endif ()
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_cxx11)

set (P_LOAD_VERSION_MAJOR 2)
set (P_LOAD_VERSION_MINOR 4)
set (P_LOAD_VERSION_PATCH 0)

set (P_LOAD_VERSION ${P_LOAD_VERSION_MAJOR}.${P_LOAD_VERSION_MINOR}.${P_LOAD_VERSION_PATCH})

if (CMAKE_VERSION VERSION_GREATER "2.8.10")
  string(TIMESTAMP YEAR "%Y")
endif ()

# Put libraries and executables in the top level of the build directory.
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Warn about everything.
set (CMAKE_C_FLAGS "-Wall -Wextra ${CMAKE_C_FLAGS}")
set (CMAKE_CXX_FLAGS "-Wall -Wextra ${CMAKE_CXX_FLAGS}")

if (WIN32)

  # Enable correct behavior for the return value of vsnprintf.
  add_definitions (-D__USE_MINGW_ANSI_STDIO=1)

endif ()

# Detect Linux.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set (LINUX 1)
endif ()

file(WRITE "${CMAKE_BINARY_DIR}/version.txt" "${P_LOAD_VERSION}")

add_subdirectory (src)

if (NOT USE_SYSTEM_TINYXML2)
  add_subdirectory (tinyxml2)
endif ()

