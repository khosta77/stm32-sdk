# Compiler and warning flags

The SDK defines its compile flags on a single CMake INTERFACE target,
`stm32_core`, in `sdk/cmake/stm32_sdk.cmake`. Every other library
(`stm32_hal`, `stm32_drivers`, `stm32_rtos`, `stm32_sensors`,
`stm32_storage`, `stm32_system`, `stm32_testing`) — and every user-app
`target_link_libraries(... stm32_core)`
in a project template — inherits the same set. There is no separate
"SDK-only" vs "user-app" warning policy.

The header-only unit-test helpers are opt-in via `STM32_USE_TESTING`, which
adds the `stm32_testing` INTERFACE target (just the `sdk/testing/include`
path — no sources, no link dependencies). See [Testing](modules/testing.md).

Within `stm32_system` the modules are conditionally compiled: `system.component`,
`system.bootstrap` and `system.work_queue` build whenever `STM32_USE_SYSTEM` is
on, while `system.executor` and `system.signal_bus` (which need the FreeRTOS
wrappers) are compiled only when `STM32_USE_FREERTOS` is also on — the queue core
stays usable in bare-metal builds.

## Configuration is Kconfig (v0.2.2)

The subsystem gates (`STM32_USE_*`), the [logging](modules/logging.md) level
and backend, the HSE frequency, the float ABI and the FreeRTOS tunables are
all configured through the project `.config` file — see
[Configuration](configuration.md). Passing them as `-D` cache variables was
removed in v0.2.2; the values below arrive from the generated
`out/generated/config.cmake`.

The log level choice is still mapped to compile definitions on the driver
target: `LOG_*` above the ceiling expand to `((void)0)` (format strings never
reach flash), and the intended sink is exposed as the `STM32_LOG_BACKEND`
define (with `STM32_LOG_BACKEND_NONE`/`_ITM`/`_UART`) so app code can pick
the matching backend type generically.

## Build environment (Docker, offline)

Since v0.1.13 all builds run inside the SDK Docker image
(`ghcr.io/khosta77/stm32-sdk-build:latest`) — `stmtool build` has no
host-toolchain path. The image pins `arm-none-eabi-gcc` 15.2 (GCC >= 14 is
required for C++20 module dependency scanning; `stm32_sdk.cmake` fails
configuration otherwise) and a host `g++` for the [host tests](modules/testing.md).

The image also **bakes FreeRTOS-Kernel** (`V11.1.0`) at `/opt/freertos-kernel`
and exports `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL`, so FreeRTOS builds no
longer re-clone the kernel per project and work offline. Outside Docker,
`stm32_rtos.cmake` still honours that variable (or environment variable) for a
pre-provisioned checkout, falling back to a shallow `FetchContent` clone.

## Active flags (v0.1.4)

```cmake
target_compile_options(stm32_core INTERFACE
    ${STM32_ARCH_FLAGS}      # -mcpu, -mfpu, -mfloat-abi from the chip family
    -Os                      # size-optimised by default; -O0 is opt-in via CMAKE_BUILD_TYPE
    -ffreestanding
    -ffunction-sections
    -fdata-sections
    -fsigned-char
    -fno-move-loop-invariants
    -Wall                    # standard set
    -Wextra
    -Wpedantic               # ISO conformance
    -Wshadow                 # catches accidental local override of outer scope
    -Werror                  # warnings ARE errors — see policy below
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>
)
```

`-Wconversion` is intentionally NOT enabled — on hand-written embedded
code that mixes 8/16/32-bit register fields, it produces a wall of
noise without finding real bugs. It may return in a future release after
a focused cleanup pass.

## `-Werror` policy

`-Werror` is permanently on starting with v0.1.4. Any warning that
reaches the compiler is a build failure, in both the SDK itself and in
projects generated via `stmtool project create`.

**Do not disable `-Werror`** to make a build go green. Instead:

1. **Fix the source** if the warning is in code you control. Most
   `-Wshadow` and `-Wpedantic` warnings have a one-line cleanup.
2. **Suppress at the file level** if the warning is in vendor code
   (CMSIS device headers, generated vector tables, FreeRTOS sources).
   Use `set_source_files_properties` next to the file in question, not
   a global `-Wno-*`. Example from the SDK:

   ```cmake
   set_source_files_properties(
       ${STM32_HAL_DIR}/src/cmsis/${STM32_VECTORS_FILE}
       PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
   )
   ```

   This keeps the warning visible everywhere else and isolates the
   exemption to a single, named file.

In downstream projects the same rule applies: if your application uses
a vendor SDK that triggers warnings, suppress them locally on those
sources, not on the whole target.

## `[[nodiscard]]` enforcement (v0.1.5)

`driver::Status` and `driver::Result<T>` are marked `[[nodiscard]]`. Combined
with `-Werror`, this turns any ignored error return into a build failure —
the compiler enforces that every `Status` is checked. When a discard is
genuinely intended (e.g. dropping a byte in a full ISR ring buffer), make it
explicit with `(void)`:

```cpp
(void) _rxBuf.push(byte);  // ISR: drop byte if RX buffer is full
```

This is a deliberate compile-time policy, not a warning to suppress. See
[drivers](modules/drivers.md#error-handling-drivertypes-v015) for `Result<T>`
and the `DRV_TRY` helpers.

## C++ language exceptions

`sdk/core/src/newlib/cxx.cpp` is the C++ runtime support translation
unit. It must be compiled with GCC 11-era C++ to match newlib's ABI
expectations. The SDK enforces this with a per-file override:

```cmake
set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/newlib/cxx.cpp
    PROPERTIES COMPILE_OPTIONS
        "-std=gnu++11;-fabi-version=0;-fno-exceptions;-fno-rtti;-fno-use-cxa-atexit;-fno-threadsafe-statics"
)
```

Everything else, including templates' `main.cpp`, compiles with C++20
(`gnu++20`).
