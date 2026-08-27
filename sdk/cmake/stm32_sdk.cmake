if(_STM32_SDK_INCLUDED)
    return()
endif()
set(_STM32_SDK_INCLUDED TRUE)

get_filename_component(_STM32_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Minimum arm-none-eabi-gcc: C++20 module dependency scanning needs GCC >= 14
# (-fdeps-format=p1689r5); GCC 13 silently fails to scan `import`. The pinned
# toolchain version is a single source of truth in docker/Dockerfile.build
# (ARG ARM_GCC_VERSION); this guard only enforces the floor. Checked here rather
# than in stm32_toolchain.cmake because CMAKE_CXX_COMPILER_VERSION is populated
# only after the compiler is identified (project()/enable_language).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR
        "arm-none-eabi-g++ ${CMAKE_CXX_COMPILER_VERSION} is too old: STM32-SDK "
        "requires GCC >= 14 for C++20 modules. Build via the SDK Docker image "
        "or upgrade your local toolchain.")
endif()

if(NOT DEFINED STM32_CHIP)
    message(FATAL_ERROR "STM32_CHIP is not defined. Set -DSTM32_CHIP=STM32F407VG (or similar)")
endif()

# Firmware content is configured exclusively through the project .config
# (Kconfig, #63): subsystem gates, log level/backend, HSE, float ABI and the
# FreeRTOS tunables all arrive from kconfig.cmake. Must run before family
# resolution -- the float ABI feeds STM32_ARCH_FLAGS.
include(${CMAKE_CURRENT_LIST_DIR}/kconfig.cmake)

# Project version from the project's own git tags -> out/generated/version.hpp
# (#90). After kconfig.cmake because it writes into the same generated dir.
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/stm32_families.cmake)
stm32_resolve_family(${STM32_CHIP})

set(STM32_HAL_DIR "${_STM32_SDK_DIR}/hal/${STM32_FAMILY_ID}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ldscripts)
configure_file(
    ${STM32_HAL_DIR}/ldscripts/mem.ld.in
    ${CMAKE_CURRENT_BINARY_DIR}/ldscripts/mem.ld
    @ONLY
)

if(NOT DEFINED STM32_HSE_VALUE)
    set(STM32_HSE_VALUE 8000000)
endif()

# Exception / RTTI dialect (#86, v0.2.4). Nothing throws at runtime -- the
# config validators signal invalid input through static_assert -- so the
# unwind tables are dead weight (~8 bytes per function in .ARM.exidx).
#
# GCC bakes the dialect into every module BMI, so a producer and a consumer
# that disagree fail with "language dialect differs 'C++20', expected
# 'C++20/no-exceptions'". These flags therefore have to reach EVERY target
# that builds or imports an SDK module: stm32_core (which the user's
# executable links) plus each OBJECT/STATIC library below, which deliberately
# do not link stm32_core. Keep them in this one variable so they cannot drift.
set(STM32_CXX_DIALECT_FLAGS
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

add_library(stm32_core INTERFACE)

target_sources(stm32_core INTERFACE
    ${_STM32_SDK_DIR}/core/src/cortexm/exception-handlers.cpp
    ${_STM32_SDK_DIR}/core/src/cortexm/initialize-hardware.cpp
    ${_STM32_SDK_DIR}/core/src/cortexm/reset-hardware.cpp
    ${_STM32_SDK_DIR}/core/src/diag/trace-impl.cpp
    ${_STM32_SDK_DIR}/core/src/diag/trace.cpp
    ${_STM32_SDK_DIR}/core/src/newlib/assert.cpp
    ${_STM32_SDK_DIR}/core/src/newlib/exit.cpp
    ${_STM32_SDK_DIR}/core/src/newlib/sbrk.cpp
    ${_STM32_SDK_DIR}/core/src/newlib/startup.cpp
    ${_STM32_SDK_DIR}/core/src/newlib/syscalls.cpp
    ${_STM32_SDK_DIR}/core/src/newlib/cxx.cpp
)

target_include_directories(stm32_core INTERFACE
    ${_STM32_SDK_DIR}/core/include
    ${_STM32_SDK_DIR}/core/include/cmsis
    ${STM32_GENERATED_DIR}
)

target_compile_options(stm32_core INTERFACE
    ${STM32_ARCH_FLAGS}
    ${STM32_CXX_DIALECT_FLAGS}
    -Os
    -ffreestanding
    -ffunction-sections
    -fdata-sections
    -fsigned-char
    -fno-move-loop-invariants
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Werror
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>
)

target_compile_definitions(stm32_core INTERFACE
    ${STM32_DEFINE}
    HSE_VALUE=${STM32_HSE_VALUE}
)

set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/newlib/startup.cpp
    PROPERTIES COMPILE_DEFINITIONS "OS_INCLUDE_STARTUP_INIT_MULTIPLE_RAM_SECTIONS"
)

set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/newlib/cxx.cpp
    PROPERTIES COMPILE_OPTIONS
        "-std=gnu++11;-fabi-version=0;-fno-use-cxa-atexit;-fno-threadsafe-statics"
)

add_library(stm32_hal INTERFACE)

target_sources(stm32_hal INTERFACE
    ${STM32_HAL_DIR}/src/cmsis/${STM32_SYSTEM_FILE}
    ${STM32_HAL_DIR}/src/cmsis/${STM32_VECTORS_FILE}
)

target_include_directories(stm32_hal INTERFACE
    ${STM32_HAL_DIR}/include
    ${STM32_HAL_DIR}/include/cmsis
)

set_source_files_properties(
    ${STM32_HAL_DIR}/src/cmsis/${STM32_SYSTEM_FILE}
    PROPERTIES COMPILE_OPTIONS "-Wno-padded"
)

set_source_files_properties(
    ${STM32_HAL_DIR}/src/cmsis/${STM32_VECTORS_FILE}
    PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
)

set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/diag/trace.cpp
    ${_STM32_SDK_DIR}/core/src/diag/trace-impl.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
)

add_library(stm32_link INTERFACE)

target_link_options(stm32_link INTERFACE
    ${STM32_ARCH_FLAGS}
    -nostartfiles
    -T${CMAKE_CURRENT_BINARY_DIR}/ldscripts/mem.ld
    -T${_STM32_SDK_DIR}/core/ldscripts/libs.ld
    -T${_STM32_SDK_DIR}/core/ldscripts/sections.ld
    --specs=nano.specs
    -Xlinker --gc-sections
    -Wl,--no-warn-rwx-segments
    -Wl,--print-memory-usage
)

# Subsystem gates come from .config (kconfig.cmake); the SENSORS/STORAGE/
# SYSTEM -> DRIVERS dependencies are enforced by `depends on` in the Kconfig
# tree, so an inconsistent combination cannot reach this point.
if(STM32_USE_FREERTOS)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_rtos.cmake)
endif()

if(STM32_USE_DRIVERS)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_drivers.cmake)
endif()

if(STM32_USE_SENSORS)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_sensors.cmake)
endif()

if(STM32_USE_STORAGE)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_storage.cmake)
endif()

if(STM32_USE_SYSTEM)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_system.cmake)
endif()

if(STM32_USE_TESTING)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_testing.cmake)
endif()
