add_library(stm32_system OBJECT)

target_sources(stm32_system PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${_STM32_SDK_DIR}
    FILES
        ${_STM32_SDK_DIR}/system/include/system/component.cppm
        ${_STM32_SDK_DIR}/system/include/system/bootstrap.cppm
        ${_STM32_SDK_DIR}/system/include/system/work_queue.cppm
)

target_include_directories(stm32_system PUBLIC
    ${_STM32_SDK_DIR}/system/include
    ${_STM32_SDK_DIR}/drivers/include
    ${STM32_HAL_DIR}/include
    ${STM32_HAL_DIR}/include/cmsis
    ${_STM32_SDK_DIR}/core/include
    ${_STM32_SDK_DIR}/core/include/cmsis
)

target_compile_features(stm32_system PUBLIC cxx_std_20)

target_compile_options(stm32_system PRIVATE
    ${STM32_ARCH_FLAGS}
    -Os
    -ffreestanding
    -ffunction-sections
    -fdata-sections
)

target_compile_definitions(stm32_system PRIVATE
    ${STM32_DEFINE}
)

target_link_libraries(stm32_system PUBLIC stm32_drivers)

# system.executor and system.signal_bus build on the FreeRTOS RAII wrappers
# (rtos.hpp). Compile them and pull in the rtos headers only when FreeRTOS is
# enabled. system.work_queue stays RTOS-free and builds unconditionally above,
# so the queue core remains usable in bare-metal super-loops.
if(STM32_USE_FREERTOS)
    target_sources(stm32_system PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${_STM32_SDK_DIR}
        FILES
            ${_STM32_SDK_DIR}/system/include/system/executor.cppm
            ${_STM32_SDK_DIR}/system/include/system/signal_bus.cppm
    )
    target_link_libraries(stm32_system PUBLIC stm32_rtos)
endif()
