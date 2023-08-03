cmake_minimum_required(VERSION 3.1)
project(concore2full CXX)

add_library(concore2full src/concore2full.cpp)
target_include_directories(concore2full PUBLIC include)

set_target_properties(concore2full PROPERTIES PUBLIC_HEADER "include/concore2full.h")

install(TARGETS concore2full)
