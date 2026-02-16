from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
from conan.tools.system.package_manager import Apt
import os

class CynlrCameraConan(ConanFile):
    name = "cynlr_camera"
    version = "1.0.0"
    license = "Proprietary"
    description = "Camera library for CynLr using Aravis and OpenCV"
    author = "CynLr"
    topics = ("camera", "aravis", "opencv", "cynlr")
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        # Force static libraries for all dependencies
        "aravis/*:shared": False,
        "opencv/*:shared": False,
    }
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*"
    
    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
    
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        
        # Ensure all dependencies are built as static libraries
        self.options["aravis"].shared = False
        self.options["opencv"].shared = False

        # Disable pcre2grep on Windows to avoid linker errors with zlib/bzip2
        if self.settings.os == "Windows":
            self.options["pcre2"].build_pcre2grep = False
    
    def system_requirements(self):
        """
        Install system-level dependencies that don't have Conan packages.
        This method runs when tools.system.package_manager:mode=install
        """
        apt = Apt(self)
        if self.settings.os == "Linux":
            apt.install([
                "libudev-dev",      # Required by libusb/libudev
                "libva-dev",        # Required by vaapi (video acceleration)
                "libvdpau-dev",     # Required by vdpau (video decode)
                "libx11-dev",       # Required by xorg
                "libxext-dev",      # Required by xorg extensions
                "libxfixes-dev",    # Required by xorg fixes
                "libxrandr-dev",    # Required by xorg randr
                "libxi-dev",        # Required by xorg input
            ], update=True, check=True)
    
    def requirements(self):
        self.requires("aravis/0.8.33", transitive_headers=True)
        self.requires("opencv/4.12.0", transitive_headers=True)
    
    def build_requirements(self):
        # Uncomment when adding proper GTest unit tests
        # self.test_requires("gtest/1.17.0")
        pass
    
    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        # Generate CMake dependency files
        deps = CMakeDeps(self)
        deps.generate()
        
        # Generate CMake toolchain
        tc = CMakeToolchain(self)
        
        # Check if tests should be skipped (Default: True)
        if self.conf.get("tools.build:skip_test", default=False, check_type=bool):
            tc.variables["BUILD_TESTING"] = "OFF"
        else:
            tc.variables["BUILD_TESTING"] = "ON"
            
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        
        # Uncomment when adding proper GTest unit tests
        # if not self.conf.get("tools.build:skip_test", default=False, check_type=bool):
        #     cmake.test()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE*", src=self.source_folder, 
             dst=os.path.join(self.package_folder, "licenses"), keep_path=False)
    
    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "cynlr_camera")
        self.cpp_info.components["camera"].libs = ["cynlr_camera"]
        self.cpp_info.components["camera"].includedirs = ["include"]
        self.cpp_info.components["camera"].requires = [
            "aravis::aravis",
            "opencv::opencv",
        ]
        self.cpp_info.components["camera"].set_property(
            "cmake_target_name", "cynlr::camera"
        )
