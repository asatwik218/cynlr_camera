cmake_minimum_required(VERSION 3.15)
project(cynlr_camera VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IntelliSense and clang-tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies via Conan
find_package(OpenCV REQUIRED)
find_package(aravis REQUIRED)

# Library source files
set(SOURCES
    src/AravisBackend.cpp
    src/AravisStream.cpp
    src/Camera.cpp
)

# Library header files
set(HEADERS
    include/AravisBackend.hpp
    include/AravisStream.hpp
    include/Camera.hpp
    include/CameraBackend.hpp
    include/Constants.hpp
    include/Error.hpp
    include/Frame.hpp
    include/Stream.hpp
)

# Create library (BUILD_SHARED_LIBS is set by Conan based on shared option)
add_library(cynlr_camera ${SOURCES} ${HEADERS})

# Set include directories
target_include_directories(cynlr_camera
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(cynlr_camera
    PUBLIC
        aravis::aravis
        opencv::opencv
)

# Export compile features - require C++20
target_compile_features(cynlr_camera PUBLIC cxx_std_20)

# Provide an alias to match consumption pattern (cynlr::camera)
add_library(cynlr::camera ALIAS cynlr_camera)

# Compiler options
if(MSVC)
    target_compile_options(cynlr_camera PRIVATE /W4)
else()
    target_compile_options(cynlr_camera PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Testing
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS cynlr_camera
    EXPORT cynlr_cameraTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets for find_package
install(EXPORT cynlr_cameraTargets
    FILE cynlr_cameraTargets.cmake
    NAMESPACE cynlr::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cynlr_camera
)