cmake_minimum_required(VERSION 3.24)
project(concore2full LANGUAGES CXX)

add_library(concore2full src/concore2full.cpp)
target_include_directories(concore2full PUBLIC include)

set_target_properties(concore2full PROPERTIES PUBLIC_HEADER "include/concore2full.h")

# Turn on warning-as-error
set_property(TARGET concore2full PROPERTY COMPILE_WARNING_AS_ERROR ON)

message(STATUS "With tests: ${WITH_TESTS}")
if(${WITH_TESTS})
     add_subdirectory(test)
endif()

install(TARGETS concore2full)
