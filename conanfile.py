from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, collect_libs

class AhoiModuleConan(ConanFile):
    name = "ahoi-module"
    version = "1.0.0-7"
    license = "MIT"
    url = "https://github.com/dmochkas/ahoi-module"
    description = "Ahoi Module"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "src/*", "include/*", "CMakeLists.txt", "config.cmake"

    options = {
        "with_logs": [True, False],
        "with_io": [True, False]
    }
    default_options = {
        "with_logs": True,
        "with_io": False
    }

    def requirements(self):
        if self.options.with_io == True:
            self.requires("io-lib/1.0.0-2@dochkas/experimental")

        if self.options.with_logs == True:
            self.requires("zlog/1.2.18@dochkas/stable")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["WITH_IO"] = bool(self.options.with_io)

        tc.variables["CMAKE_TRY_COMPILE_TARGET_TYPE"] = "STATIC_LIBRARY"
        tc.variables["WITH_LOGS"] = bool(self.options.with_logs)
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
