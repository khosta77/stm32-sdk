include(FetchContent)

set(FREERTOS_KERNEL_VERSION "V11.1.0" CACHE STRING "FreeRTOS Kernel version tag")

# Prefer a pre-provisioned FreeRTOS-Kernel checkout over a per-project clone.
# The SDK Docker image bakes the kernel at /opt/freertos-kernel and exports
# FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL, so builds are offline and skip the
# repeated shallow clone (#47). FetchContent honours the CMake variable of that
# name natively; bridge the environment variable set by the image into it.
if(NOT FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL
   AND DEFINED ENV{FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL})
    set(FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL
        "$ENV{FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL}"
        CACHE PATH "Pre-provisioned FreeRTOS-Kernel source directory")
endif()

FetchContent_Declare(
    freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        ${FREERTOS_KERNEL_VERSION}
    GIT_SHALLOW    TRUE
)

# CMP0169 (deprecation of single-arg FetchContent_Populate) only exists in CMake
# >= 3.30; the image ships 3.28 (the SDK baseline for C++20 modules), where the
# policy is unknown and the call is not deprecated. Guard so both versions work.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(freertos_kernel)

if(STM32_ARCH_FLAGS MATCHES "mfloat-abi=hard" OR STM32_ARCH_FLAGS MATCHES "mfloat-abi=softfp")
    set(_FREERTOS_PORT_DIR "${freertos_kernel_SOURCE_DIR}/portable/GCC/ARM_CM4F")
else()
    set(_FREERTOS_PORT_DIR "${freertos_kernel_SOURCE_DIR}/portable/GCC/ARM_CM4")
endif()

add_library(stm32_rtos STATIC
    ${freertos_kernel_SOURCE_DIR}/tasks.c
    ${freertos_kernel_SOURCE_DIR}/queue.c
    ${freertos_kernel_SOURCE_DIR}/list.c
    ${freertos_kernel_SOURCE_DIR}/timers.c
    ${freertos_kernel_SOURCE_DIR}/event_groups.c
    ${freertos_kernel_SOURCE_DIR}/stream_buffer.c
    ${_FREERTOS_PORT_DIR}/port.c
    ${freertos_kernel_SOURCE_DIR}/portable/MemMang/heap_4.c
    ${_STM32_SDK_DIR}/rtos/src/freertos_hooks.cpp
)

target_include_directories(stm32_rtos PUBLIC
    ${freertos_kernel_SOURCE_DIR}/include
    ${_FREERTOS_PORT_DIR}
    ${_STM32_SDK_DIR}/rtos/include
    ${_STM32_SDK_DIR}/rtos/include/rtos
)

target_include_directories(stm32_rtos PRIVATE
    ${_STM32_SDK_DIR}/core/include
    ${_STM32_SDK_DIR}/core/include/cmsis
    ${STM32_HAL_DIR}/include
    ${STM32_HAL_DIR}/include/cmsis
)

target_compile_options(stm32_rtos PRIVATE
    ${STM32_ARCH_FLAGS}
    -Os
    -ffreestanding
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>
)

target_compile_definitions(stm32_rtos PRIVATE
    ${STM32_DEFINE}
)

target_compile_definitions(stm32_rtos PUBLIC
    STM32_USE_FREERTOS
)

target_link_options(stm32_rtos PUBLIC
    -Wl,--whole-archive $<TARGET_FILE:stm32_rtos> -Wl,--no-whole-archive
)
