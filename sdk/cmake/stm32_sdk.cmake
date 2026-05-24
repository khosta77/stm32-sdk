if(_STM32_SDK_INCLUDED)
    return()
endif()
set(_STM32_SDK_INCLUDED TRUE)

get_filename_component(_STM32_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED STM32_CHIP)
    message(FATAL_ERROR "STM32_CHIP is not defined. Set -DSTM32_CHIP=STM32F407VG (or similar)")
endif()

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

add_library(stm32_core INTERFACE)

target_sources(stm32_core INTERFACE
    ${_STM32_SDK_DIR}/core/src/cortexm/exception-handlers.c
    ${_STM32_SDK_DIR}/core/src/cortexm/initialize-hardware.c
    ${_STM32_SDK_DIR}/core/src/cortexm/reset-hardware.c
    ${_STM32_SDK_DIR}/core/src/diag/trace-impl.c
    ${_STM32_SDK_DIR}/core/src/diag/trace.c
    ${_STM32_SDK_DIR}/core/src/newlib/assert.c
    ${_STM32_SDK_DIR}/core/src/newlib/exit.c
    ${_STM32_SDK_DIR}/core/src/newlib/sbrk.c
    ${_STM32_SDK_DIR}/core/src/newlib/startup.c
    ${_STM32_SDK_DIR}/core/src/newlib/syscalls.c
    ${_STM32_SDK_DIR}/core/src/newlib/cxx.cpp
)

target_include_directories(stm32_core INTERFACE
    ${_STM32_SDK_DIR}/core/include
    ${_STM32_SDK_DIR}/core/include/cmsis
)

target_compile_options(stm32_core INTERFACE
    ${STM32_ARCH_FLAGS}
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
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>
)

target_compile_definitions(stm32_core INTERFACE
    ${STM32_DEFINE}
    HSE_VALUE=${STM32_HSE_VALUE}
)

set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/newlib/startup.c
    PROPERTIES COMPILE_DEFINITIONS "OS_INCLUDE_STARTUP_INIT_MULTIPLE_RAM_SECTIONS"
)

set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/newlib/cxx.cpp
    PROPERTIES COMPILE_OPTIONS
        "-std=gnu++11;-fabi-version=0;-fno-exceptions;-fno-rtti;-fno-use-cxa-atexit;-fno-threadsafe-statics"
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
    ${_STM32_SDK_DIR}/core/src/diag/trace.c
    ${_STM32_SDK_DIR}/core/src/diag/trace-impl.c
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

option(STM32_USE_FREERTOS "Enable FreeRTOS RTOS support" OFF)
option(STM32_USE_DRIVERS "Enable peripheral driver library" OFF)
option(STM32_USE_SENSORS "Enable sensor library" OFF)
option(STM32_USE_STORAGE "Enable flash storage library" OFF)

if(STM32_USE_FREERTOS)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_rtos.cmake)
endif()

if(STM32_USE_DRIVERS)
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_drivers.cmake)
endif()

if(STM32_USE_SENSORS)
    if(NOT STM32_USE_DRIVERS)
        message(FATAL_ERROR "STM32_USE_SENSORS requires STM32_USE_DRIVERS=ON")
    endif()
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_sensors.cmake)
endif()

if(STM32_USE_STORAGE)
    if(NOT STM32_USE_DRIVERS)
        message(FATAL_ERROR "STM32_USE_STORAGE requires STM32_USE_DRIVERS=ON")
    endif()
    include(${CMAKE_CURRENT_LIST_DIR}/stm32_storage.cmake)
endif()
