# Header-only on-device unit-test helpers (testing/unit_test.hpp).
# No sources, no link dependencies -- just the include path. The same header
# compiles on the host too, so a test file can be shared between host and device.
#
# Reusable concept-satisfying mock buses (issue #34) ship as a standalone C++20
# module at testing/mock/mock_bus.cppm (module `testing.mock`, exporting
# testing::MockI2c / MockSpi / MockUart / MockGpioPin / MockFlash). It is not
# added to this INTERFACE target on purpose: it imports the driver concept
# modules, so a consumer opts in by adding the file to its own CXX_MODULES file
# set alongside stm32_drivers. The host test tree (tests/host) does exactly that.
add_library(stm32_testing INTERFACE)

target_include_directories(stm32_testing INTERFACE
    ${_STM32_SDK_DIR}/testing/include
)

target_compile_features(stm32_testing INTERFACE cxx_std_20)
