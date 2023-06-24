# concore2full
Core abstractions for dealing with concurrency in C++, using stackfull coroutines


## Building

The following tools are needed:
* [`conan`](https://www.conan.io/)
* [`CMake`](https://cmake.org/)

Perform the following actions:
```
mkdir -p build
cd build

# Build and test the library
conan create ..
```

Alternatively, instead of the `conan create` command, one may run:
```
# Get all the dependencies and generate the build scripts
conan install .. --build=missing -s build_type=Release
# Build the project
conan build ..
```
