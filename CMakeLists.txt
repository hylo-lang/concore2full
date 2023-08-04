cmake_minimum_required(VERSION 3.20)
project(concore2full LANGUAGES CXX)

find_package(Catch2 REQUIRED CONFIG)

add_library(concore2full src/concore2full.cpp)
target_include_directories(concore2full PUBLIC include)

set_target_properties(concore2full PROPERTIES PUBLIC_HEADER "include/concore2full.h")

# if($ENV{CONAN_RUN_TESTS})
#   message(STATUS "Building with tests")
#   add_subdirectory(test)
# endif()

install(TARGETS concore2full)
