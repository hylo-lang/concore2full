# The source files for the functional tests
set(sourceFiles
"test_smoke.cpp"
)

find_package(Catch2 REQUIRED CONFIG)

# Add the tests executable
add_executable(test.concore2full ${sourceFiles})
target_link_libraries(test.concore2full PRIVATE concore2full Catch2::Catch2WithMain)
target_include_directories(test.concore2full PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Discover the Catch2 test built by the application
include(CTest)
include(Catch)
catch_discover_tests(test.concore2full)

# Turn all warnings
target_compile_options(test.concore2full PUBLIC
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)
