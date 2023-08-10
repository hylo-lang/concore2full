cmake_minimum_required(VERSION 3.24)
project(concore2full LANGUAGES CXX)

find_package(context_core_api CONFIG REQUIRED)

add_library(concore2full src/concore2full.cpp)
target_include_directories(concore2full PUBLIC include)
set_target_properties(concore2full PROPERTIES PUBLIC_HEADER "include/concore2full.h")
target_link_libraries(concore2full PUBLIC context_core_api::context_core_api)

# Turn on warning-as-error
set_property(TARGET concore2full PROPERTY COMPILE_WARNING_AS_ERROR ON)

message(STATUS "With tests: ${WITH_TESTS}")
if(${WITH_TESTS})
     add_subdirectory(test)
endif()

install(TARGETS concore2full)
