from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from os import getenv

class Concore2fullRecipe(ConanFile):
    name = "concore2full"
    version = "0.1.0"

    # Optional metadata
    license = "MIT"
    author = "Lucian Radu Teodorescu"
    url = "https://github.com/lucteo/concore2full"
    description = "Core abstractions for dealing with concurrency in C++, using stackfull coroutines"
    topics = ("concurrency", "C++", "Val", "stackfull coroutines")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False], "with_tests": [True, False]}
    default_options = {"shared": False, "fPIC": True, "with_tests": False}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    def build_requirements(self):
        self.requires("catch2/3.4.0")
        self.requires("context_core_api/1.0.0")

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["WITH_TESTS"] = self.options.with_tests
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["concore2full"]

# from <root>/build/ directory, run:
#   > conan install .. --build=missing -s compiler.cppstd=17 -o with_tests=True
#
# then:
#   > conan build .. -s compiler.cppstd=17
#
# publish and test the package with:
#   > conan export ..
#   > conan test ../test_package concore2full/0.1.0 --build=missing
#
# or, to run everything in one go:
#   > conan create .. --build=missing -s compiler.cppstd=17
#
# Note: changing `with_tests` value requires deleting the temporary build files.
