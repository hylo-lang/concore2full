# The source files for the functional tests
set(sourceFiles
"test_smoke.cpp"
"test_simple_example.cpp"
"test_stack_allocator.cpp"
"test_callcc.cpp"
"test_spawn.cpp"
"test_bulk_spawn.cpp"
"test_thread_pool.cpp"
"test_sync_execute.cpp"
"test_suspend.cpp"
"example_conc_sort.cpp"
"example_skynet.cpp"
"example_async_io.cpp"
"sketch_split.cpp"
# "sketch_cancellation.cpp"
"tests_c.cpp"
"c/test_spawn.c"
"c/test_bulk_spawn.c"
)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

# Enable this to disable the POSIX signals in Catch2
# target_compile_definitions(Catch2 PUBLIC CATCH_CONFIG_NO_POSIX_SIGNALS)

# Add the tests executable
add_executable(test.concore2full ${sourceFiles})
target_link_libraries(test.concore2full PRIVATE concore2full Catch2::Catch2WithMain)
target_include_directories(test.concore2full PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${concore2full_SOURCE_DIR}/include")

# Turn on warning-as-error
set_property(TARGET test.concore2full PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Discover the Catch2 test built by the application
include(CTest)
catch_discover_tests(test.concore2full PROPERTIES TIMEOUT 30)
