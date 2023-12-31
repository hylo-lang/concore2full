from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os

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
    options = {"shared": [True, False], "fPIC": [True, False], "with_tests": [True, False], "with_tracy": [True, False]}
    default_options = {"shared": False, "fPIC": True, "with_tests": False, "with_tracy": False}
    build_policy = "missing"   # Some of the dependencies don't have builds for all our targets

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    def build_requirements(self):
        self.requires("catch2/3.4.0")
        self.requires("context_core_api/1.0.0")
        if self.options.with_tracy:
            self.test_requires("tracy-interface/0.1.0")

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
        tc.variables["WITH_TRACY"] = self.options.with_tracy
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, pattern="*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
        copy(self, pattern="*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, pattern="*.so", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, pattern="*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, pattern="*.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
        copy(self, pattern="*.dylib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["concore2full"]

# from <root>/build/ directory, run:
#   > conan install .. --build=missing -s compiler.cppstd=20 -o with_tests=True
#
# then:
#   > conan build .. -s compiler.cppstd=20
#
# publish and test the package with:
#   > conan export ..
#   > conan test ../test_package concore2full/0.1.0 --build=missing
#
# or, to run everything in one go:
#   > conan create .. --build=missing -s compiler.cppstd=20
#
# Note: changing `with_tests` value requires deleting the temporary build files.
