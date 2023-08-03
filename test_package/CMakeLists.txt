cmake_minimum_required(VERSION 3.15)
project(PackageTest CXX)

find_package(concore2full CONFIG REQUIRED)

add_executable(example src/example.cpp)
target_link_libraries(example concore2full::concore2full)