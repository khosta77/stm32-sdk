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
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/clock.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/dma.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/gpio.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/i2c.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/spi.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/flash.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/uart.cppm
        ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/exti.cppm
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
    -Os
    -ffreestanding
    -ffunction-sections
    -fdata-sections
)

target_compile_definitions(stm32_drivers PRIVATE
    ${STM32_DEFINE}
)

# Logging facility (issues #36, #37). STM32_LOG_LEVEL is the compile-time
# ceiling: LOG_* above it vanish from the image. STM32_LOG_BACKEND names the
# intended sink (both backend modules always compile -- templates and inline
# CMSIS code cost nothing until instantiated), exposed as a define so app code
# can pick the matching backend type generically.
set(STM32_LOG_LEVEL "INFO"
    CACHE STRING "Compile-time max log level (NONE/ERROR/WARN/INFO/DEBUG/TRACE)")
set_property(CACHE STM32_LOG_LEVEL PROPERTY STRINGS
    NONE ERROR WARN INFO DEBUG TRACE)
set(STM32_LOG_BACKEND "none"
    CACHE STRING "Log backend selected by the build (none/itm/uart)")
set_property(CACHE STM32_LOG_BACKEND PROPERTY STRINGS none itm uart)

set(_stm32_log_levels NONE ERROR WARN INFO DEBUG TRACE)
list(FIND _stm32_log_levels "${STM32_LOG_LEVEL}" _stm32_log_level_num)
if(_stm32_log_level_num LESS 0)
    message(FATAL_ERROR
        "STM32_LOG_LEVEL='${STM32_LOG_LEVEL}' invalid; expected one of: "
        "${_stm32_log_levels}")
endif()

if(STM32_LOG_BACKEND STREQUAL "none")
    set(_stm32_log_backend_num 0)
elseif(STM32_LOG_BACKEND STREQUAL "itm")
    set(_stm32_log_backend_num 1)
elseif(STM32_LOG_BACKEND STREQUAL "uart")
    set(_stm32_log_backend_num 2)
else()
    message(FATAL_ERROR
        "STM32_LOG_BACKEND='${STM32_LOG_BACKEND}' invalid; expected none/itm/uart")
endif()

target_compile_definitions(stm32_drivers PUBLIC
    STM32_LOG_LEVEL=${_stm32_log_level_num}
    STM32_LOG_BACKEND=${_stm32_log_backend_num}
    STM32_LOG_BACKEND_NONE=0
    STM32_LOG_BACKEND_ITM=1
    STM32_LOG_BACKEND_UART=2
)

if(STM32_USE_FREERTOS)
    target_link_libraries(stm32_drivers PRIVATE stm32_rtos)
    target_compile_definitions(stm32_drivers PUBLIC STM32_USE_FREERTOS)
endif()
