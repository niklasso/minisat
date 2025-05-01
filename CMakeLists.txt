cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(minisat LANGUAGES CXX CUDA)

#–– Options
option(STATIC_BINARIES  "Link binaries statically."  ON)
option(USE_SORELEASE    "Use SORELEASE in shared library filename."  ON)

#–– Versioning
set(MINISAT_SOMAJOR    2)
set(MINISAT_SOMINOR    1)
set(MINISAT_SORELEASE  0)
if (USE_SORELEASE)
  set(MINISAT_VERSION     ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR}.${MINISAT_SORELEASE})
else()
  set(MINISAT_VERSION     ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR})
endif()
set(MINISAT_SOVERSION   ${MINISAT_SOMAJOR})

#–– Dependencies
find_package(ZLIB   REQUIRED)
find_package(CUDA   REQUIRED)

include_directories(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/gpu
  ${ZLIB_INCLUDE_DIR}
  ${CUDA_INCLUDE_DIRS}
)

#–– Compiler settings
set(CMAKE_CXX_STANDARD     14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD    14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)

#–– Source lists
file(GLOB MINISAT_CORE_SOURCES
     minisat/utils/Options.cc
     minisat/utils/System.cc
     minisat/core/Solver.cc
     minisat/simp/SimpSolver.cc
)

file(GLOB GPU_KERNEL_SOURCES
     gpu/*.cu
)

#–– Make sure our .cu files are treated as CUDA
set_source_files_properties(
  ${GPU_KERNEL_SOURCES}
  PROPERTIES LANGUAGE CUDA
)

#–– Build the library (static + shared) with both .cc and .cu
add_library(minisat-lib-static  STATIC ${MINISAT_CORE_SOURCES} ${GPU_KERNEL_SOURCES})
add_library(minisat-lib-shared  SHARED ${MINISAT_CORE_SOURCES} ${GPU_KERNEL_SOURCES})

target_link_libraries(minisat-lib-static PRIVATE ZLIB::ZLIB ${CUDA_CUDART_LIBRARY})
target_link_libraries(minisat-lib-shared PRIVATE ZLIB::ZLIB ${CUDA_CUDART_LIBRARY})

set_target_properties(minisat-lib-static PROPERTIES OUTPUT_NAME "minisat")
set_target_properties(minisat-lib-shared
  PROPERTIES OUTPUT_NAME "minisat"
             VERSION     ${MINISAT_VERSION}
             SOVERSION   ${MINISAT_SOVERSION}
)

#–– Executables
add_executable(minisat_core minisat/core/Main.cc)
add_executable(minisat_simp minisat/simp/Main.cc)
# add_executable(test_gpu_vs_cpu tests/test_gpu_vs_cpu.cpp)

# Mark the .cc mains as CUDA if you ever embed kernels there
set_source_files_properties(
  minisat/core/Main.cc
  minisat/simp/Main.cc
  PROPERTIES LANGUAGE CUDA
)

# Link CPU solvers
if (STATIC_BINARIES)
  target_link_libraries(minisat_core PRIVATE minisat-lib-static ${CUDA_CUDART_LIBRARY})
  target_link_libraries(minisat_simp PRIVATE minisat-lib-static ${CUDA_CUDART_LIBRARY})
else()
  target_link_libraries(minisat_core PRIVATE minisat-lib-shared ${CUDA_CUDART_LIBRARY})
  target_link_libraries(minisat_simp PRIVATE minisat-lib-shared ${CUDA_CUDART_LIBRARY})
endif()

# Link the test against the static library + CUDA runtime
# target_link_libraries(test_gpu_vs_cpu PRIVATE minisat-lib-static ${CUDA_CUDART_LIBRARY})

#–– Install rules
install(TARGETS
    minisat-lib-static
    minisat-lib-shared
    minisat_core
    minisat_simp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY minisat/mtl
                 minisat/utils
                 minisat/core
                 minisat/simp
        DESTINATION include/minisat
        FILES_MATCHING PATTERN "*.h"
)
