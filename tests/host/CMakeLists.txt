cmake_minimum_required(VERSION 3.28)
project(stm32_sdk_host_tests LANGUAGES CXX)

# Host (x86) build of the CMSIS-free portable layer plus the reusable mock
# buses, so driver/sensor logic runs as native unit tests with no hardware
# (issues #34, #35). Built and executed inside the SDK Docker image, which
# provides g++-14 (C++20 module scanning needs GCC >= 14).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # gnu++20: mpu6050 uses __asm volatile("nop")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

enable_testing()

get_filename_component(SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk" ABSOLUTE)

add_library(sdk_portable STATIC)
target_sources(sdk_portable PUBLIC
  FILE_SET modules TYPE CXX_MODULES BASE_DIRS ${SDK} FILES
    ${SDK}/drivers/include/driver/types.cppm
    ${SDK}/drivers/include/driver/log.cppm
    ${SDK}/drivers/include/driver/circular_buffer.cppm
    ${SDK}/drivers/include/driver/interface/i_i2c.cppm
    ${SDK}/drivers/include/driver/interface/i_spi.cppm
    ${SDK}/drivers/include/driver/interface/i_uart.cppm
    ${SDK}/drivers/include/driver/interface/i_gpio.cppm
    ${SDK}/drivers/include/driver/interface/i_flash.cppm
    ${SDK}/drivers/include/driver/interface/i_crc.cppm
    ${SDK}/drivers/include/driver/soft_crc.cppm
    ${SDK}/storage/include/storage/geometry.cppm
    ${SDK}/storage/include/storage/partition.cppm
    ${SDK}/sensors/include/sensor/imu.cppm
    ${SDK}/sensors/include/sensor/display.cppm
    ${SDK}/sensors/include/sensor/external_flash.cppm
    ${SDK}/sensors/accelerometers/mpu6050/mpu6050.cppm
    ${SDK}/sensors/displays/ssd1306/font5x7.cppm
    ${SDK}/sensors/displays/ssd1306/ssd1306.cppm
    ${SDK}/sensors/storage/w25q32/w25q32.cppm
    ${SDK}/testing/mock/mock_bus.cppm
)
target_compile_features(sdk_portable PUBLIC cxx_std_20)
target_include_directories(sdk_portable PUBLIC
  ${SDK}/drivers/include
  ${SDK}/testing/include
)
target_compile_options(sdk_portable PRIVATE -Wall -Wextra)

function(add_host_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE sdk_portable)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_host_test(test_result)
add_host_test(test_mock)
add_host_test(test_sensor_mock)
add_host_test(test_log)
add_host_test(test_circular_buffer)
add_host_test(test_w25q32)
add_host_test(test_ssd1306)
add_host_test(test_crc)
add_host_test(test_partition)
