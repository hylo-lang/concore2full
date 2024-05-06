cmake_minimum_required(VERSION 3.24)
project(concore2full LANGUAGES CXX C)

add_subdirectory(external/context-core-api)

find_package(Threads REQUIRED)

set(Sources
src/profiling.cpp
src/spawn.cpp
src/bulk_spawn.cpp
src/this_thread.cpp
src/thread_info.cpp
src/thread_pool.cpp
src/thread_snapshot.cpp
src/thread_suspension.cpp
)

add_library(concore2full ${Sources})
target_include_directories(concore2full PUBLIC include)
target_link_libraries(concore2full PUBLIC context_core_api ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(concore2full PUBLIC cxx_std_20)

# Turn on warning-as-error
set_property(TARGET concore2full PROPERTY COMPILE_WARNING_AS_ERROR ON)

message(STATUS "With tracy: ${WITH_TRACY}")
if(${WITH_TRACY})
     find_package(tracy-interface CONFIG REQUIRED)
     target_link_libraries(concore2full PUBLIC tracy-interface::tracy-interface)
     target_compile_definitions(concore2full PUBLIC USE_TRACY=1)
endif()

option(WITH_TESTS "Build the tests" OFF)
message(STATUS "With tests: ${WITH_TESTS}")
if(${WITH_TESTS})
     add_subdirectory(test)
endif()
