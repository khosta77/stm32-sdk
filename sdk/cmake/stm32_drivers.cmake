# Family-specific driver modules live next to the family, not in this file
# (#98): sdk/drivers/families/<family>.cmake sets STM32_DRIVER_FAMILY_MODULES.
set(_stm32_driver_family_file
    "${_STM32_SDK_DIR}/drivers/families/${STM32_FAMILY_ID}.cmake")
if(NOT EXISTS "${_stm32_driver_family_file}")
    message(FATAL_ERROR
        "No driver modules for family ${STM32_FAMILY_ID}.\n"
        "Expected: ${_stm32_driver_family_file}\n"
        "Adding a family: see docs/chips/index.md")
endif()
include("${_stm32_driver_family_file}")

add_library(stm32_drivers OBJECT)

target_sources(stm32_drivers PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${_STM32_SDK_DIR}
    FILES
        ${_STM32_SDK_DIR}/drivers/include/driver/types.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/reg.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/circular_buffer.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_gpio.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/null_gpio.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_uart.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_i2c.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_spi.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_flash.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_exti.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/interface/i_crc.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/soft_crc.cppm
        ${STM32_DRIVER_FAMILY_MODULES}
        ${_STM32_SDK_DIR}/drivers/include/driver/log.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/log_backend_itm.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/log_backend_uart.cppm
)

target_include_directories(stm32_drivers PUBLIC
    ${_STM32_SDK_DIR}/drivers/include
    ${STM32_HAL_DIR}/include
    ${STM32_HAL_DIR}/include/cmsis
    ${_STM32_SDK_DIR}/core/include
    ${_STM32_SDK_DIR}/core/include/cmsis
)

target_compile_features(stm32_drivers PUBLIC cxx_std_20)

target_compile_options(stm32_drivers PRIVATE
    ${STM32_ARCH_FLAGS}
    ${STM32_CXX_DIALECT_FLAGS}
    -Os
    -ffreestanding
    -ffunction-sections
    -fdata-sections
)

target_compile_definitions(stm32_drivers PRIVATE
    ${STM32_DEFINE}
)

# Logging facility (issues #36, #37). The compile-time level ceiling and the
# intended sink are Kconfig choices (#63); kconfig.cmake maps them to the
# numeric STM32_LOG_LEVEL_NUM / STM32_LOG_BACKEND_NUM used here. LOG_* above
# the ceiling vanish from the image; both backend modules always compile --
# templates and inline CMSIS code cost nothing until instantiated.
target_compile_definitions(stm32_drivers PUBLIC
    STM32_LOG_LEVEL=${STM32_LOG_LEVEL_NUM}
    STM32_LOG_BACKEND=${STM32_LOG_BACKEND_NUM}
    STM32_LOG_BACKEND_NONE=0
    STM32_LOG_BACKEND_ITM=1
    STM32_LOG_BACKEND_UART=2
)

if(STM32_USE_FREERTOS)
    target_link_libraries(stm32_drivers PRIVATE stm32_rtos)
    target_compile_definitions(stm32_drivers PUBLIC STM32_USE_FREERTOS)
endif()
