# The pinned arm-none-eabi-gcc version is a single source of truth in
# docker/Dockerfile.build (ARG ARM_GCC_VERSION). The GCC >= 14 minimum required
# for C++20 module scanning is enforced in stm32_sdk.cmake, once the compiler is
# identified (CMAKE_CXX_COMPILER_VERSION is not yet set while this file is read).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_C_COMPILER arm-none-eabi-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++ REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(CMAKE_SIZE arm-none-eabi-size REQUIRED)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
