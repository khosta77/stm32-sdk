add_library(stm32_storage OBJECT)

target_sources(stm32_storage PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${_STM32_SDK_DIR}
    FILES
        ${_STM32_SDK_DIR}/storage/include/storage/geometry.cppm
        ${_STM32_SDK_DIR}/storage/include/storage/partition.cppm
        ${_STM32_SDK_DIR}/storage/include/storage/flash_device.cppm
)

target_include_directories(stm32_storage PUBLIC
    ${_STM32_SDK_DIR}/storage/include
)

target_compile_features(stm32_storage PUBLIC cxx_std_20)

target_compile_options(stm32_storage PRIVATE
    ${STM32_ARCH_FLAGS}
    ${STM32_CXX_DIALECT_FLAGS}
    -Os
    -ffreestanding
    -ffunction-sections
    -fdata-sections
)

target_compile_definitions(stm32_storage PRIVATE
    ${STM32_DEFINE}
)

# Storage needs the driver concepts (driver.types, driver.flash, driver.crc)
# but deliberately NOT stm32_sensors: the external-flash adapter states its own
# structural requirements, so the partition layer works with STM32_USE_SENSORS
# off.
target_link_libraries(stm32_storage PUBLIC stm32_drivers)
