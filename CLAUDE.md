# stm32-sdk

Bare-metal C++20 ecosystem for STM32 (CMSIS only).

## Usage

```bash
pip install ./tools/stmtool
stmtool project create my-project --chip STM32F407VG
cd my-project
stmtool build --native
stmtool flash
```

## Code rules

- CMSIS only, HAL/LL forbidden.
- C++20 (gnu++20), C11 (gnu11), `cxx.cpp` compiled with gnu++11.
- Google Code Style (clang-format).
- Conventional Commits in English.
- No raw pointers to MMIO — references (`GPIO_TypeDef&`).
- MMIO register access only through `driver::reg::set` / `reg::clear` /
  `reg::read` / `reg::write` / `reg::get` / `reg::modify`. Raw `|=`, `&=`,
  `& flag`, `= value` on `volatile uint32_t` (`RCC->...`, `_periph.CR1`,
  `_stream->NDTR`, etc.) are forbidden. Local `uint32_t` bit accumulators
  before a single `reg::write` are allowed.
- Config structs without default values — every field is specified explicitly.
- Global peripheral objects (no static-local, no pointers).

## Documentation maintenance (must follow)

Any change to a public surface of the SDK **must** update the corresponding
page in `docs/` in the same PR. This applies to drivers, sensors, the
`stmtool` CLI, and these rules. Touch both the English source (`*.md`) and
the Russian translation (`*.ru.md`).

- New / modified driver → `docs/modules/drivers.md` + `docs/modules/drivers.ru.md`.
- New / modified sensor → `docs/sensors/<name>.md` + `.ru.md`; if the
  interface (`IDisplay`, `IExternalFlash`, ...) changes, also touch
  `docs/modules/sensors.md`.
- New `stmtool` command or flag → `docs/stmtool.md` + `.ru.md`, and update
  the table in `README.md`.
- New / changed rule in this file → mirror it into the per-template
  `CLAUDE.md.template` files where it applies to user-side code.
- Anything that requires action from downstream projects on upgrade → add
  a section to `docs/migration.md` + `.ru.md`.

PRs without the corresponding documentation update should not merge. The
`docs.yml` workflow rebuilds the site on every push to `develop`; failing
to update docs means future contributors see stale information.

## Quality enforcement (must follow)

- `-Werror` is permanently enabled in `sdk/cmake/stm32_sdk.cmake` via the
  `stm32_core` INTERFACE target. Warnings are errors in SDK code and in
  downstream user projects. Do NOT disable `-Werror` or any individual
  `-W*` flag without explicit user approval. If a CMSIS / vendor header
  triggers a warning, suppress it locally at the file level via
  `set_source_files_properties(... PROPERTIES COMPILE_OPTIONS "-Wno-XXX")`,
  not globally.

- `stmtool` code must pass `poetry run poe ci` before any commit: ruff,
  flake8, black, isort, mypy (strict), bandit, pylint, pytest with at
  least 70% coverage. Do NOT add `# noqa`, `# type: ignore`,
  `--ignore=...`, or `disable=...` directives without explicit user
  approval. If a linter produces a critically large number of errors,
  stop and ask the user whether to disable the rule or fix every
  occurrence -- never decide unilaterally.

- `poetry run poe fix` runs all auto-fixers (ruff, isort, black, ruff
  format). It is safe to run locally; results must still pass `poe ci`.

- Commit messages are in English (Conventional Commits). Pull request
  descriptions targeting `develop` are in Russian. This split keeps the
  history machine-readable for tooling while the PR narrative stays
  natural for the team.

## Release process

Releases use SemVer (`vMAJOR.MINOR.PATCH`). The project is pre-1.0; both
patch and minor releases may include source-side changes. Where the patch /
minor line falls is at the maintainer's discretion until v1.0.

To cut a release:

1. Make sure `develop` is green (matrix build + docs build).
2. Tag the merge commit on `develop`:
   `git tag -a vMAJOR.MINOR.PATCH -m "Release vMAJOR.MINOR.PATCH" <sha>`.
3. `git push origin vMAJOR.MINOR.PATCH`.
4. Create the GitHub Release; copy notes from the new section in
   `docs/release.md`.

The version number is derived from git tags via
`poetry-dynamic-versioning` (the SDK side still picks the same tag) --
no hand-edited version constants anywhere. Tagging the repo is what
publishes a release for `stmtool` consumers.

## Project templates and CLAUDE.md

`stmtool project create <name> --chip <chip> --template <tpl> --with-claude`
copies `templates/<category>/<tpl>/CLAUDE.md.template` into the generated
project as `CLAUDE.md`, with `@PROJECT_NAME@` / `@STM32_CHIP@` /
`@SDK_VERSION@` substituted.

Each template has its own `CLAUDE.md.template` — they share a common
header (SDK rules, build/flash commands, docs links) but the per-template
section covers the specifics: which peripherals are used, pinout,
expected serial output, how to verify on hardware.

If you add a new template, add a `CLAUDE.md.template` alongside its
`template.toml`. The `--with-claude` path emits a warning when the file
is missing rather than failing — but every shipped template should have
one.

## C++20 modules

Architecture: interface modules (`.cppm`) + implementation modules (`.cppm`).

### Module pattern

```cpp
module;
#include <cstdint>              // STL — ONLY via #include in global module fragment
#include "cmsis/stm32f4xx.h"    // CMSIS — also #include (macros are not exported)
#ifdef STM32_USE_FREERTOS
#include "FreeRTOS.h"
#endif
export module driver.stm32f4.i2c;

import driver.types;             // our modules — through import
import driver.i2c;
import driver.reg;
```

### Why #include and not import

- `import <cstdint>;` (header units) — GCC 15 arm-none-eabi does not support them.
- `import std;` — C++23, not supported.
- CMake does not support header units.
- `#include` in global module fragment is the standard C++20 pattern.

### TU-local limitations for templates

CMSIS `__STATIC_INLINE` functions (`NVIC_SetPriority`, `__DSB`) are
`static inline` = TU-local. Templates (`Uart<>`) cannot call them directly.
Solution: wrappers with module linkage.

```cpp
namespace driver::stm32f4::detail {
void dsb() { __DSB(); }
void nvicEnableIRQ(IRQn_Type irq) { NVIC_EnableIRQ(irq); }
}
```

Non-template classes (`GpioPin`, `I2c`, `Spi`) can call CMSIS functions directly.

### Don't mix #include <span> and import

GCC 15 emits redefinition errors if `main.cpp` does `#include <span>` and
also `import`s a module that includes `<span>` in its global fragment.
Solution: don't include STL headers in `main.cpp`; use brace-init
`{ptr, len}` to construct a span.

## Layout

- `sdk/core/` — ARM CMSIS core, Cortex-M runtime, newlib, linker scripts.
- `sdk/hal/stm32f4/` — STM32F4 device headers, vector tables, memory layout.
- `sdk/cmake/` — `stm32_sdk.cmake`, `stm32_toolchain.cmake`, `families/`.
- `sdk/rtos/` — FreeRTOS (heap_4, 16 KB), RAII wrappers (`rtos.hpp`).
- `sdk/drivers/include/driver/` — modules:
    - `types.cppm`, `reg.cppm`, `circular_buffer.cppm` — utilities.
    - `interface/i_*.cppm` — interfaces (`IGpioPin`, `IUart`, `II2c`, `ISpi`, `IFlash`).
    - `stm32f4/*.cppm` — implementations (`GpioPin`, `Uart<>`, `I2c`, `Spi`,
      `DmaStream`, `InternalFlash`, `clock`).
- `sdk/sensors/` — sensor interfaces + implementations (MPU6050, SSD1306, W25Q32).
- `templates/` — 7 project templates (`bare-metal/blink`, `bare-metal/i2c-scan`,
  `freertos/blink`, `freertos/mpu6050-uart`, `freertos/oled-display-test`,
  `freertos/w25q32-flash-test`, `freertos/imu-flash-oled-demo`).
- `tools/stmtool/` — Python CLI tool (Typer + Rich).
- `tools/docs/` — MkDocs build dependencies.
- `docs/` — MkDocs source (EN + RU via suffix mode).

## Driver patterns

### Clock enable

- `GpioPin`: enables GPIO port clock in its constructor (if/else on port address).
- I2C / UART / SPI / DMA: clocks enabled via `__initialize_hardware()` override
  in `main.cpp` before C++ constructors run.

### Config

All configs have no defaults. The caller fills every field:

```cpp
I2c::Config{ .clockSpeed = 400000, .fastMode = true }
Uart<>::Config{ .baudrate = 115200, .dataBits = 8, .stopBits = 1, .parity = Parity::None }
```

### `GpioConfig` — aggregate + consteval validation via `gpio()`

`GpioConfig` is a plain aggregate (`af = 0` is the only default). Validation is
factored out into a free `consteval` function `gpio()`:

```cpp
GpioPin g_led{
    *GPIOD,
    gpio({
        .pin = 12,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
```

`gpio({...})` throws (at compile time) on `pin > 15`, `mode == None`,
missing `speed`/`type` for Output / AF, `af > 15` for AF. Because it's
`consteval`, calling with runtime values is rejected by the compiler — it's
impossible to accidentally pass an unvalidated config.

Trailing comma after the last field (`.type = ...,`) is idiomatic: clang-format
preserves multi-line formatting and won't collapse the call into one line.

### `Uart` — template buffer sizes and mode

```cpp
Uart<512, 256> uart{...};                          // interrupt mode, default
Uart<512, 256, UartMode::Dma> uartDma{...};        // DMA TX
```

`static_assert`s enforce: buffer sizes are powers of two, minimum 16. Mode is
opt-in through the third template parameter; existing call sites that don't
specify it remain on the interrupt path.

### DMA — `DmaStream` RAII + `dmaMap` table

`driver.stm32f4.dma` ships `DmaStream` (RAII wrapper around CMSIS DMA stream
registers) plus a `dmaMap` namespace with peripheral ↔ stream/channel
constants for USART1/2/3/6, UART4/5, SPI1/2/3 on F407VG. New DMA-enabled
drivers go through this layer.

## FreeRTOS patterns

### Global objects

```cpp
extern "C" void __initialize_hardware() {
    SystemCoreClockUpdate();
    driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIODEN);
    driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN);
    __DSB();
}

namespace {
GpioPin g_led{*GPIOD, gpio({.pin = 12, .mode = PinMode::Output, ...})};
I2c g_i2c1{*I2C1, {.clockSpeed = 400000, .fastMode = true}};
Uart<> g_uart2{*USART2, USART2_IRQn, {...}};
}
```

`__initialize_hardware()` is weak in the SDK; the application overrides it.
It runs before C++ constructors.

### Objects created after `startScheduler`

`CachedSensor` and `rtos::Task` are `static` (the MSP is reused for ISRs
once the scheduler starts).

### ISR handler

```cpp
extern "C" void USART2_IRQHandler() { g_uart2.irqHandler(); }
```

Direct reference to the global object (not a pointer). Null-check semaphores
inside ISRs to guard against spurious interrupts.

## CMake

- `-nostartfiles` is mandatory (without it newlib `crt0` overrides our `_start`).
- `nano.specs` — no `%f` support (need `-u _printf_float` or integer formatting).
- `stm32_drivers`: OBJECT library, doesn't link `stm32_hal`/`stm32_core`
  (INTERFACE sources would duplicate).
- CMSIS include dirs are added directly to `stm32_drivers`.

## Chip selection

`-DSTM32_CHIP=<name>` where name = `STM32F4xxYZ` (Y = package, Z = flash size
letter). Supported: F401, F405, F407, F411, F412, F429, F439, F446. The chip
database is in `sdk/cmake/families/stm32f4.cmake`.

## Adding a new family

1. Create `sdk/cmake/families/stm32XX.cmake` with `stm32XX_get_chip_info`.
2. Add CMSIS device headers under `sdk/hal/stm32XX/include/cmsis/`.
3. Add vector tables under `sdk/hal/stm32XX/src/cmsis/`.
4. Create `sdk/hal/stm32XX/ldscripts/mem.ld.in`.

Don't forget to document the new family on `docs/chips/index.md` and the
family-specific page.
