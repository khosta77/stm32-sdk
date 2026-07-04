# Compiler and warning flags

The SDK defines its compile flags on a single CMake INTERFACE target,
`stm32_core`, in `sdk/cmake/stm32_sdk.cmake`. Every other library
(`stm32_hal`, `stm32_drivers`, `stm32_rtos`, `stm32_sensors`,
`stm32_storage`, `stm32_system`) — and every user-app
`target_link_libraries(... stm32_core)`
in a project template — inherits the same set. There is no separate
"SDK-only" vs "user-app" warning policy.

Within `stm32_system` the modules are conditionally compiled: `system.component`,
`system.bootstrap` and `system.work_queue` build whenever `STM32_USE_SYSTEM` is
on, while `system.executor` and `system.signal_bus` (which need the FreeRTOS
wrappers) are compiled only when `STM32_USE_FREERTOS` is also on — the queue core
stays usable in bare-metal builds.

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
