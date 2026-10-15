# TGVC top-level build.
#
# Scope of this file: stand up the C++ build and the CTest harness so every later
# ticket can land a failing test before its implementation. The full source layout,
# MLIR integration, dependency policy and CI wiring are completed by TF-006, TF-007
# and TF-009.
cmake_minimum_required(VERSION 3.28)

project(TGVC
  VERSION 0.1.0
  DESCRIPTION "Tensor graph to Vulkan compiler"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(TGVC_WARNINGS_AS_ERRORS "Treat warnings in TGVC code as errors" ON)
option(TGVC_ENABLE_SANITIZERS "Build with address and undefined-behaviour sanitizers" OFF)

# Warnings are opt-in per target so third-party headers stay out of the way.
add_library(tgvc_warnings INTERFACE)
target_compile_options(tgvc_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow>
)
if(TGVC_WARNINGS_AS_ERRORS)
  target_compile_options(tgvc_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
  )
endif()

# Sanitizers are wired but off by default; TF-009 turns them on in CI.
add_library(tgvc_sanitizers INTERFACE)
if(TGVC_ENABLE_SANITIZERS)
  target_compile_options(tgvc_sanitizers INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=address,undefined;-fno-omit-frame-pointer>
  )
  target_link_options(tgvc_sanitizers INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=address,undefined>
  )
endif()

include(CTest) # defines BUILD_TESTING and enables `ctest`
if(BUILD_TESTING)
  include(cmake/Dependencies.cmake)
  add_subdirectory(tests)
endif()
