import os
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.scm import Version
from conan.tools.files import apply_conandata_patches, export_conandata_patches, get, rmdir, copy, mkdir
from conan.tools.cmake import CMakeToolchain, CMake, CMakeDeps, cmake_layout

required_conan_version = ">=1.52.0"

class RotorConan(ConanFile):
    name = "rotor"
    license = "MIT"
    homepage = "https://github.com/basiliscos/cpp-rotor"
    url = "https://github.com/conan-io/conan-center-index"
    description = (
        "Event loop friendly C++ actor micro-framework, supervisable"
    )
    topics = ("concurrency", "actor-framework", "actors", "actor-model", "erlang", "supervising", "supervisor")
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "test_package/*", "cmake/*", "tests/*", "examples/*"

    test_requires = "catch2/3.4.0"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "fPIC": [True, False],
        "shared": [True, False],
        "enable_asio": [True, False],
        "enable_ev" : [True, False],
        "enable_fltk" : [True, False],
        "enable_thread": [True, False],
        "multithreading": [True, False],  # enables multithreading support
    }
    default_options = {
        "fPIC": True,
        "shared": False,
        "enable_asio": True,
        "enable_ev" : False,
        "enable_fltk" : False,
        "enable_thread": True,
        "multithreading": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            try:
                del self.options.fPIC
            except Exception:
                pass

    def requirements(self):
        self.requires("boost/1.83.0", transitive_headers=True)
        if self.options.enable_ev:
            self.requires("libev/4.33")
        if self.options.enable_fltk:
            self.requires("fltk/1.3.9")

#    def layout(self):
#        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_BOOST_ASIO"] = self.options.enable_asio
        tc.variables["BUILD_THREAD"] = self.options.enable_thread
        tc.variables["BUILD_EV"] = self.options.enable_ev
        tc.variables["BUILD_FLTK"] = self.options.enable_fltk
        tc.variables["BUILD_EXAMPLES"] = os.environ.get('ROTOR_BUILD_EXAMPLES', 'OFF')
        tc.variables["BUILD_THREAD_UNSAFE"] = not self.options.multithreading
        tc.variables["BUILD_TESTING"] = not self.conf.get("tools.build:skip_test", default=True, check_type=bool)
        tc.generate()
        tc = CMakeDeps(self)
        tc.generate()

        if (self.settings.os == "Windows"):
            bin_dir = os.path.join(self.build_folder, "bin")
            mkdir(self, bin_dir)
            for dep in self.dependencies.values():
                copy(self, "*.dll", dep.cpp_info.bindir, bin_dir)

    def validate(self):
        minimal_cpp_standard = "17"
#        if self.settings.compiler.get_safe("cppstd"):
#            check_min_cppstd(self, minimal_cpp_standard)
        minimal_version = {
            "gcc": "7",
            "clang": "6",
            "apple-clang": "10",
            "Visual Studio": "15"
        }
        compiler = str(self.settings.compiler)
        if compiler not in minimal_version:
            print(
                f"{self.ref} recipe lacks information about the {compiler} compiler standard version support")
            print(
                f"{self.ref} requires a compiler that supports at least C++{minimal_cpp_standard}")
            return

        compiler_version = Version(self.settings.compiler.version)
        if compiler_version < minimal_version[compiler]:
            raise ConanInvalidConfiguration(f"{self.ref} requires a compiler that supports at least C++{minimal_cpp_standard}")

        if self.options.shared and Version(self.version) < "0.23":
            raise ConanInvalidConfiguration("shared option is available from v0.23")


    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=True):
            cmake.test()

    def package(self):
        copy(self, pattern="LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.components["core"].libs = ["rotor"]
        self.cpp_info.components["core"].requires = ["boost::date_time", "boost::system", "boost::regex"]

        if not self.options.multithreading:
            self.cpp_info.components["core"].defines.append("BUILD_THREAD_UNSAFE")

        if self.options.enable_asio:
            self.cpp_info.components["asio"].libs = ["rotor_asio"]
            self.cpp_info.components["asio"].requires = ["core"]

        if self.options.enable_thread:
            self.cpp_info.components["thread"].libs = ["rotor_thread"]
            self.cpp_info.components["thread"].requires = ["core"]

        if self.options.enable_ev:
            self.cpp_info.components["ev"].libs = ["rotor_ev"]
            self.cpp_info.components["ev"].requires = ["core", "libev::libev"]

        if self.options.enable_fltk:
            self.cpp_info.components["fltk"].libs = ["rotor_fltk"]
            self.cpp_info.components["fltk"].requires = ["core", "fltk::fltk"]
