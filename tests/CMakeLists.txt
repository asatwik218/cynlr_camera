# CMakeLists.txt for test directory

# ------------------------------------------------------------------
# Standalone hardware integration tests (require a physical camera)
# These are NOT GTest unit tests — they have their own main() and
# run infinite acquisition loops. They are built but NOT registered
# with CTest so they don't break CI.
# ------------------------------------------------------------------
add_executable(aravisTest standalone/aravisTest/aravisTest.cpp)
add_executable(cynlrCamTest standalone/cynlrCamTest/cynlrCamTest.cpp)
add_executable(lensFocusTest standalone/lensFocusTest/lensFocusTest.cpp)
add_executable(fileAccessTest standalone/fileAccessTest/fileAccessTest.cpp)

set_target_properties(aravisTest cynlrCamTest lensFocusTest fileAccessTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

target_link_libraries(aravisTest PRIVATE cynlr::camera)
target_link_libraries(cynlrCamTest PRIVATE cynlr::camera)
target_link_libraries(lensFocusTest PRIVATE cynlr::camera)
target_link_libraries(fileAccessTest PRIVATE cynlr::camera)

target_compile_features(aravisTest PRIVATE cxx_std_20)
target_compile_features(cynlrCamTest PRIVATE cxx_std_20)
target_compile_features(lensFocusTest PRIVATE cxx_std_20)
target_compile_features(fileAccessTest PRIVATE cxx_std_20)

# ------------------------------------------------------------------
# GTest unit tests (run in CI without hardware)
# Add proper GTest-based tests below this line.
# ------------------------------------------------------------------
# find_package(GTest REQUIRED)
# include(GoogleTest)
#
# Example:
# add_executable(cameraUnitTest unit/cameraUnitTest.cpp)
# target_link_libraries(cameraUnitTest PRIVATE cynlr::camera GTest::gtest GTest::gtest_main)
# target_compile_features(cameraUnitTest PRIVATE cxx_std_20)
# gtest_discover_tests(cameraUnitTest)