cmake_minimum_required(VERSION 3.24)
project(concore2full LANGUAGES CXX C)

add_subdirectory(external/context-core-api)

find_package(Threads REQUIRED)

set(Sources
src/profiling.cpp
src/spawn.cpp
src/spawn_frame_base.cpp
src/copyable_spawn_frame_base.cpp
src/bulk_spawn_frame_base.cpp
src/this_thread.cpp
src/sleep_helper.cpp
src/thread_info.cpp
src/thread_pool.cpp
src/thread_snapshot.cpp
src/suspend.cpp
)

add_library(concore2full ${Sources})
target_include_directories(concore2full PUBLIC include)
target_link_libraries(concore2full PUBLIC context_core_api ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(concore2full PUBLIC cxx_std_20)

# Turn on warning-as-error
set_property(TARGET concore2full PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Turn on experimental library features for libc++
include(CheckCXXSymbolExists)
if(cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  set(header version)
else()
  set(header ciso646)
endif()
check_cxx_symbol_exists(_LIBCPP_VERSION ${header} LIBCPP)
if(LIBCPP)
     target_compile_options(concore2full PUBLIC -fexperimental-library)
endif()

set(PROFILING_LITE_PATH "INVALID" CACHE PATH "Path towards profiling-lite")
if(EXISTS "${PROFILING_LITE_PATH}/cxx")
     message(STATUS "Using profiling-lite from: ${PROFILING_LITE_PATH}")
     target_compile_definitions(concore2full PUBLIC USE_PROFILING_LITE=1)
     target_compile_definitions(concore2full PUBLIC PROFILING_LITE_PATH="${PROFILING_LITE_PATH}")
     target_include_directories(concore2full PUBLIC "${PROFILING_LITE_PATH}/cxx")
endif()

option(WITH_TESTS "Build the tests" OFF)
message(STATUS "With tests: ${WITH_TESTS}")
if(${WITH_TESTS})
     add_subdirectory(test)
endif()
