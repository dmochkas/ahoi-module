from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, collect_libs

class AhoiModuleConan(ConanFile):
    name = "ahoi-module"
    version = "1.0.0-1"
    license = "MIT"
    url = "https://github.com/dmochkas/ahoi-module"
    description = "Ahoi module"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "src/*", "include/*", "CMakeLists.txt"

    default_options = {}

    def requirements(self):
        self.requires("io-lib/1.0.0-1@dochkas/experimental")

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
        self.cpp_info.libs = collect_libs(self)
