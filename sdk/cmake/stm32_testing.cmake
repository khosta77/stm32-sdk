# Header-only on-device unit-test helpers (testing/unit_test.hpp).
# No sources, no link dependencies -- just the include path. The same header
# compiles on the host too, so a test file can be shared between host and device.
add_library(stm32_testing INTERFACE)

target_include_directories(stm32_testing INTERFACE
    ${_STM32_SDK_DIR}/testing/include
)

target_compile_features(stm32_testing INTERFACE cxx_std_20)
