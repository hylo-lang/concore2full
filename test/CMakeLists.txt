# The source files for the functional tests
set(sourceFiles
"test_smoke.cpp"
"test_simple_example.cpp"
"test_stack_allocator.cpp"
"test_callcc.cpp"
"test_spawn.cpp"
)

find_package(Catch2 REQUIRED CONFIG)

# Add the tests executable
add_executable(test.concore2full ${sourceFiles})
target_link_libraries(test.concore2full PRIVATE concore2full Catch2::Catch2WithMain)
target_include_directories(test.concore2full PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${concore2full_SOURCE_DIR}/include")

# Turn on warning-as-error
set_property(TARGET test.concore2full PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Discover the Catch2 test built by the application
include(CTest)
include(Catch)
catch_discover_tests(test.concore2full)
