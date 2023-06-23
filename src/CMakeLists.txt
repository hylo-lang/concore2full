cmake_minimum_required(VERSION 3.1)
project(concore2full CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_library(concore2full concore2full.cpp)
