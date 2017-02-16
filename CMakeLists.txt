cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

project(minisat)

#--------------------------------------------------------------------------------------------------
# Configurable options:

option(STATIC_BINARIES "Link binaries statically." ON)
option(USE_SORELEASE   "Use SORELEASE in shared library filename." ON)

#--------------------------------------------------------------------------------------------------
# Library version:

set(MINISAT_SOMAJOR   2)
set(MINISAT_SOMINOR   1)
set(MINISAT_SORELEASE 0)

# Compute VERSION and SOVERSION:
if (USE_SORELEASE)
  set(MINISAT_VERSION ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR}.${MINISAT_SORELEASE})
else()
  set(MINISAT_VERSION ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR})
endif()
set(MINISAT_SOVERSION ${MINISAT_SOMAJOR})

#--------------------------------------------------------------------------------------------------
# Dependencies:

find_package(ZLIB)
include_directories(${ZLIB_INCLUDE_DIR})
include_directories(${minisat_SOURCE_DIR})

#--------------------------------------------------------------------------------------------------
# Optimization:

# Change the default build type from Debug to Release, while still
# supporting overriding the build type.
if (NOT CMAKE_BUILD_TYPE)
  message("-- No build type specified; defaulting to CMAKE_BUILD_TYPE=Release.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING
    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
    FORCE)
endif (NOT CMAKE_BUILD_TYPE)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  add_definitions(-DNDEBUG)

  if (CMAKE_COMPILER_IS_GNUCXX)
      message("-- Optimizing for release mode.")
      # Linux
      if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        message("-- Linux")
        add_definitions("-march=native -mtune=native")
      endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      # Cygwin
      if (${CMAKE_SYSTEM_NAME} STREQUAL "CYGWIN")
        message("-- Cygwin")
        add_definitions("-march=native -mtune=native")
      endif (${CMAKE_SYSTEM_NAME} STREQUAL "CYGWIN")
      # Mac OS X
      if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        message("-- MacOS")
        add_definitions("-fast -msse3")
      endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  endif (CMAKE_COMPILER_IS_GNUCXX)
endif (CMAKE_BUILD_TYPE STREQUAL "Release")

#--------------------------------------------------------------------------------------------------
# Compile flags:

add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)

#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MINISAT_LIB_SOURCES
    minisat/utils/Options.cc
    minisat/utils/System.cc
    minisat/core/Solver.cc
    minisat/simp/SimpSolver.cc)

add_library(minisat-lib-static STATIC ${MINISAT_LIB_SOURCES})
add_library(minisat-lib-shared SHARED ${MINISAT_LIB_SOURCES})

target_link_libraries(minisat-lib-shared ${ZLIB_LIBRARY})
target_link_libraries(minisat-lib-static ${ZLIB_LIBRARY})

add_executable(minisat_core minisat/core/Main.cc)
add_executable(minisat_simp minisat/simp/Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(minisat_core minisat-lib-static)
  target_link_libraries(minisat_simp minisat-lib-static)
else()
  target_link_libraries(minisat_core minisat-lib-shared)
  target_link_libraries(minisat_simp minisat-lib-shared)
endif()

set_target_properties(minisat-lib-static PROPERTIES OUTPUT_NAME "minisat")
set_target_properties(minisat-lib-shared
  PROPERTIES
    OUTPUT_NAME "minisat" 
    VERSION ${MINISAT_VERSION}
    SOVERSION ${MINISAT_SOVERSION})

set_target_properties(minisat_simp       PROPERTIES OUTPUT_NAME "minisat")

#--------------------------------------------------------------------------------------------------
# Installation targets:

install(TARGETS minisat-lib-static minisat-lib-shared minisat_core minisat_simp 
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY minisat/mtl minisat/utils minisat/core minisat/simp
        DESTINATION include/minisat
        FILES_MATCHING PATTERN "*.h")
