# Family-specific driver modules for STM32F4 (#98).
#
# stm32_drivers.cmake includes the file matching STM32_FAMILY_ID and appends
# STM32_DRIVER_FAMILY_MODULES to its FILE_SET. Adding a family means dropping a
# sibling file here plus the modules it lists -- no edit to stm32_drivers.cmake.
#
# Order matters: a module must be listed after every module it imports.
set(STM32_DRIVER_FAMILY_MODULES
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/clock.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/dma.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/gpio.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/i2c.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/spi.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/flash.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/uart.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/exti.cppm
    ${_STM32_SDK_DIR}/drivers/include/driver/stm32f4/crc.cppm
)
