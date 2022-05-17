from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout


class FederliebConan(ConanFile):
    name = "federlieb"
    version = "1"

    # Optional metadata
    license = "MIT"
    author = "Bjoern Hoehrmann <bjoern@hoehrmann.de>"
    url = "https://github.com/federlieb/federlieb/"
    description = "C++23 bindings for SQLite"
    topics = ("sqlite", "binding")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
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
        self.cpp_info.libs = ["federlieb"]
