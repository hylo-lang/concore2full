# concore2full
Core abstractions for dealing with concurrency in C++, using stackful coroutines


## Building

The following tools are needed:
* [`CMake`](https://cmake.org/)

1. **Configure step**

    Execute the following:
    ```
    cmake -G Ninja -S . -B <build-directory> -D CMAKE_BUILD_TYPE=<build-type> -DWITH_TESTS=On \
    ```
    where `<build-directory>` is a build directory (usually `build` or `.build`), and `<build-type>` is the build type (usually `Debug` or `Release`).

2. **Build step**
    ```
    cmake --build <build-directory>
    ```

3. **Test step**

    For this step, `WITH_TESTS` needs to be set to `ON` in the configure step.

    ```
    ctest --test-dir <build-directory>/test
    ```
