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
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    # @property
    # def _run_tests(self):
    #     return getenv("CONAN_RUN_TESTS", False)

    def build_requirements(self):
        # if self._run_tests:
        self.build_requires("catch2/3.4.0")

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
