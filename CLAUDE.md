# stm32-sdk

Bare-metal C++20 ecosystem for STM32 (CMSIS only).

## Usage

```bash
pipx install git+https://github.com/khosta77/stmtool.git
stmtool project create my-project --chip STM32F407VG
cd my-project
stmtool config  # Kconfig menuconfig TUI, edits .config
stmtool build   # always runs in the SDK Docker image; artifacts in out/
stmtool flash
```

Since v0.2.1 `stmtool` lives in its own repository
(`khosta77/stmtool`), not in this tree. Any change to the tool goes there and is
released as a `v0.N` tag; this SDK consumes it by installing from that repo. Do
NOT re-add `tools/stmtool/` here.

Since v0.1.13 all builds run inside the SDK Docker image (no `--native`);
artifacts land in `out/`. `stmtool test` builds and runs the host unit tests
(`tests/host` on the mock buses) in the image. The image pins the ARM toolchain
(GCC 15.2; `stm32_sdk.cmake` requires GCC >= 14) and bakes FreeRTOS-Kernel.

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

## Configuration (Kconfig, since v0.2.2)

The project-local `.config` is the **single source of truth** for firmware
content (hard cut — the old `set(STM32_USE_*)`/`-D` path was removed):

- Tree: `sdk/Kconfig` + per-subsystem fragments (`sdk/rtos/Kconfig`,
  `sdk/drivers/Kconfig`, ...). Codegen: `sdk/cmake/kconfig.cmake` runs
  `sdk/scripts/kconfig/genconfig.py` (kconfiglib, baked into both Docker
  images) → `out/generated/config.cmake` + `out/generated/stm32_autoconf.h`
  (read by `FreeRTOSConfig.h`) + `out/generated/Kconfig.chip`.
- `stmproject.toml` keeps only workspace concerns: `[sdk] version`,
  `[target] chip`, `[flash] tool` (west model — chip/SDK ref must exist
  before the tree can be parsed). `[project]`/`[build]`/`[serial]` are gone.
- Templates ship a `defconfig` (gate set only); `stmtool project create`
  copies it into the project as `.config`. No `set(STM32_CHIP)`/
  `set(STM32_USE_*)` in template CMakeLists.
- A new peripheral driver MUST bring its options in its subsystem Kconfig
  fragment: `depends on STM32_USE_DRIVERS`, a `help` text and a safe default
  on every symbol; invariants via `range`/`depends on`, strict inequalities
  via `_cross_checks` in `genconfig.py`. genconfig is strict: any rejected
  `.config` assignment fails the configure step.
- FreeRTOS tunables (`FREERTOS_*`: heap, tick, priorities, stacks) are
  Kconfig symbols; defaults reproduce the historic hardcoded values.
  `FREERTOS_TIMER_TASK_PRIORITY` must stay strictly below
  `FREERTOS_MAX_PRIORITIES`; demos create tasks at priority 2.

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

- C/C++ style is Google, **2-space indent, 80-column limit**,
  `Cpp11BracedListStyle: true` (braced-init lists format as `{nullptr}`),
  fixed in the root `.clang-format` (since v0.1.12). clang-format is
  **pinned to 19.1.3** in `.pre-commit-config.yaml` and the `Format` CI
  job; use clang-format 19 locally so output matches. Do NOT change the
  style or the pin without explicit user approval. Vendor CMSIS / newlib
  sources are excluded via `.clang-format-ignore` and must stay pristine —
  never reformat them.

- pre-commit hooks are mandatory (`.pre-commit-config.yaml`): clang-format
  on C/C++/`.cppm` and Conventional Commits on the message. Install once with
  `pre-commit install --hook-type pre-commit --hook-type commit-msg`. The
  `Format` workflow enforces the same clang-format on every push / PR to
  `develop`. Contributor workflow lives in `CONTRIBUTING.md`.

- `stmtool` is a **separate repository** (`khosta77/stmtool`) since v0.2.1. Its
  Python quality gate (`poe ci`: ruff/flake8/black/isort/mypy/bandit/pylint/
  pytest) and its own `CLAUDE.md` live there — do not edit tool code in this
  repo. Changes to the tool are made and released in that repo.

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
4. Create the GitHub Release with **bilingual** notes: an `## English`
   section copied from the new block in `docs/release.md`, then a
   `## Русский` section copied from `docs/release.ru.md`, separated by
   `---`. Both come verbatim from the docs so the Release page and the
   docs site stay in sync. Finish with a footer linking the docs release
   page and the `vPREV...vNEW` changelog compare.

The SDK version number is derived from git tags via
`poetry-dynamic-versioning` -- no hand-edited version constants anywhere.
Tagging the repo is what publishes an SDK release. `stmtool` versions itself
independently in its own repo (`v0.N`, CI-autotagged) and is not tied to the
SDK tag stream anymore.

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
#include <cstdint>  // STL — ONLY via #include in global module fragment
#include "cmsis/stm32f4xx.h"  // CMSIS — also #include (macros are not exported)
#ifdef STM32_USE_FREERTOS
#include "FreeRTOS.h"
#endif
export module driver.stm32f4.i2c;

import driver.types; // our modules — through import
import driver.i2c;
import driver.reg;
```

### Why #include and not import

- `import <cstdint>;` (header units) — GCC 15 arm-none-eabi does not support them.
- `import std;` — C++23, not supported.
- CMake does not support header units.
- `#include` in global module fragment is the standard C++20 pattern.

### Macros live in textual headers, not modules

Macros are not exported across `import`. Any user-facing macro must ship as a
textual `#include` header, included after the relevant `import`, not defined
inside a `.cppm`. Current helper headers:

- `driver/try.hpp` — `DRV_TRY` / `DRV_TRY_ASSIGN` early-return helpers.
- `util/thread_safety.hpp` — Clang thread-safety annotation wrappers
  (`GUARDED_BY`, `REQUIRES`, `ACQUIRE`, ...), no-op on GCC (v0.1.11).
- `testing/unit_test.hpp` — `ASSERT_*` / `EXPECT_*` on-device unit-test
  helpers + `TestRunner` (v0.1.11).

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

- `sdk/core/` — ARM CMSIS core, Cortex-M runtime, newlib, linker scripts. Our
  own glue (`src/cortexm/*.cpp`, `src/diag/*.cpp`) plus the newlib glue
  (`src/newlib/*.cpp` — `assert`/`exit`/`sbrk`/`startup`/`syscalls`/`cxx`) is
  idiomatic C++ since v0.1.15–v0.1.16, with `extern "C"` boundaries for every
  vector-table / newlib / kernel symbol and `main` declared outside the block
  (never mangled). All of `src/` is clang-formatted like the rest of the SDK;
  no `src/` subtree remains in `.clang-format-ignore` (only vendor CMSIS device
  headers under `include/cmsis` are ignored).
- `sdk/hal/stm32f4/` — STM32F4 device headers, memory layout, plus the vendor
  runtime sources `src/cmsis/system_stm32f4xx.cpp` and the per-family
  `vectors_*.cpp` — C++ since v0.1.16 (`extern "C"` wrap, ST/µOS++ logic
  verbatim; `__isr_vectors` forced back to external linkage). The CMSIS device
  headers (`include/cmsis/*.h`) stay `.h` and pristine — renaming them to
  `.hpp` would break ST's internal cross-includes and upstream diffability.
- `sdk/cmake/` — `stm32_sdk.cmake`, `stm32_toolchain.cmake`, `families/`.
- `sdk/rtos/` — FreeRTOS (heap_4, 16 KB), RAII wrappers (`rtos.hpp`).
- `sdk/drivers/include/driver/` — modules:
    - `types.cppm`, `reg.cppm`, `circular_buffer.cppm` — utilities.
    - `interface/i_*.cppm` — interface **concepts** (`IGpioPin`, `IUart`,
      `II2c`, `ISpi`, `IFlash`, `IExti`), not virtual classes (since v0.1.7).
    - `stm32f4/*.cppm` — implementations (`GpioPin`, `Uart<>`, `I2c`, `Spi`,
      `DmaStream`, `InternalFlash`, `ExtiLine`, `clock`); model the concepts
      without inheritance, each ends with `static_assert(IXxx<Impl>)`.
    - `log.cppm` + textual `log.hpp` (since v0.1.14) — `driver.log`: compile-time
      `LOG_*` macros (`STM32_LOG_LEVEL` strips levels to `((void)0)`), runtime
      `setLevel`, sink installed via a `void(*)(void*, const char*, size_t)`
      thunk (WorkItem style). Backends `log_backend_itm.cppm` (`ItmBackend`, calls
      CMSIS `ITM_SendChar` directly — non-template, like `GpioPin`) and
      `log_backend_uart.cppm` (`UartBackend<UartDriver>` over `driver.uart`).
      `STM32_LOG_BACKEND` (none/itm/uart) names the build-selected sink. Core
      module is CMSIS-free → host-tested (`tests/host/test_log.cpp`).
- `sdk/sensors/` — sensor concepts (`IImu`/`IDisplay`/`IExternalFlash`) +
  implementations. Sensors are templates on their bus type
  (`template <driver::II2c I2cDriver> class Mpu6050`); MPU6050, SSD1306, W25Q32.
- `sdk/system/` — component framework (since v0.1.8, `STM32_USE_SYSTEM`):
  `component.cppm` (`system::Component` concept + `ComponentBase` +
  `ComponentState`/`Criticality`), `bootstrap.cppm` (variadic
  `bootstrap(...)` + `BootReport`). Zero vtable, depends only on `driver.types`.
  Concurrency layer (v0.1.9): `work_queue.cppm` (intrusive `WorkItem` +
  `WorkQueue`, RTOS-free, CMSIS PRIMASK), `executor.cppm`
  (`SingleThreadExecutor` on an `rtos::Task`), `timer.cppm` (`Timer` over the
  executor, v0.1.10) and `signal_bus.cppm` (type-safe
  `Channel<Event, MaxSubs, RingDepth>`) — all but `work_queue` only under
  FreeRTOS.
- `sdk/testing/` — on-device unit-test helpers (since v0.1.11,
  `STM32_USE_TESTING`): `testing/unit_test.hpp` (`ASSERT_*`/`EXPECT_*` +
  `TestRunner`, no exceptions/heap, injected `Writer`; same code host + device),
  linked as INTERFACE `stm32_testing`. Plus `testing/mock/mock_bus.cppm` (module
  `testing.mock`, v0.1.13): reusable programmable mocks `MockI2c`/`MockSpi`/
  `MockUart`/`MockGpioPin`/`MockFlash` satisfying the driver concepts.
- `tests/host/` — standalone CMake tree (v0.1.13) built with the image's host
  `g++`: compiles the CMSIS-free portable modules + `testing.mock` into `ctest`
  executables (`test_result`/`test_mock`/`test_sensor_mock`/`test_log`/
  `test_circular_buffer`/`test_w25q32`/`test_ssd1306`). Run via `stmtool test`;
  a `host-tests` CI job runs the same. `system.work_queue`/`system.signal_bus`
  still pull CMSIS/FreeRTOS and are not host-portable yet.
- `templates/` — 10 project templates (`bare-metal/blink`, `bare-metal/i2c-scan`,
  `bare-metal/unit-test-demo`, `freertos/blink`, `freertos/mpu6050-uart`,
  `freertos/oled-display-test`, `freertos/w25q32-flash-test`,
  `freertos/imu-flash-oled-demo`, `freertos/signal-bus-demo`,
  `freertos/button-events-demo`).
- `stmtool` — the Python CLI (Typer + Rich) is a **separate repository**
  (`khosta77/stmtool`) since v0.2.1; not vendored here.
- `tools/docs/` — MkDocs build dependencies.
- `docs/` — MkDocs source (EN + RU via suffix mode).

## Driver patterns

### Interfaces are concepts, not virtual classes (since v0.1.7)

`interface/i_*.cppm` and `sensor/{imu,display,external_flash}.cppm` export
C++20 **concepts** (`IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash`, `IImu`,
`IDisplay`, `IExternalFlash`), checked with `std::same_as` on each method
(`<concepts>` works freestanding on arm-none-eabi GCC 15). Rules for new code:

- A driver/sensor **models** the concept by providing the methods — no
  inheritance, no `override`. End the module with `static_assert(IXxx<Impl>)`.
- A consumer that needs a generic bus is a constrained template
  (`template <driver::II2c I2cDriver> class Mpu6050 { I2cDriver &_i2c; ... }`). CTAD keeps
  call sites unchanged (`sensor::Mpu6050 g{i2c, {...}}`); cast `void*` back with
  `decltype(&g)`, not `sensor::Mpu6050*` (a template name needs args).
- Style: use the **terse constrained type-parameter** form
  (`template <driver::II2c I2cDriver>`), NOT `requires`-clauses or
  `driver::II2c auto&`. Give the parameter a full descriptive name —
  `I2cDriver` / `SpiDriver` / `GpioDriver` — never a terse `Bus` / `T`.
- Every value-returning method on a driver/sensor is `[[nodiscard]]` (not only
  the `Status` type — also `size_t`/`uint*` counts and geometry). Intentional
  discards (e.g. a fire-and-forget `Uart::write`) use explicit `(void)`.
- Non-template constants of a templated class go in a sibling non-template
  struct (`sensor::W25q32Spec::SECTOR_SIZE`).
- A test/mock bus is a plain `struct` that satisfies the concept; the sensor
  modules keep a `namespace sensor::detail` mock + self-check.
- Avoid range-for over `std::span` inside a **template** member: GCC 15 can't
  resolve the iterator's `operator!=` across a module boundary at instantiation.
  Use an index loop.

### Clock enable

- `GpioPin`: enables GPIO port clock in its constructor (if/else on port address).
- I2C / UART / SPI / DMA: clocks enabled via `__initialize_hardware()` override
  in `main.cpp` before C++ constructors run.

### Config — free structs + `consteval` validators (since v0.1.11)

All configs have no defaults; the caller fills every field. Since v0.1.11 the
I2C/SPI/UART configs live in their interface module as **free** structs
(`driver::I2cConfig`/`SpiConfig`/`UartConfig`, no longer nested `I2c::Config`)
with a free `consteval` validator each — same shape as `gpio()`/`exti()`. An
out-of-range or unset field fails to compile. SPI/UART fields are strong enums
(`SpiMode`, `SpiDataSize`, `DataBits`, `StopBits`) with a `None` sentinel the
validator rejects:

```cpp
I2c g_i2c{*I2C1, i2c({.clockSpeed = 400000, .fastMode = true})};
Spi g_spi{
    *SPI2,
    spi(
        {.clockHz = 10000000,
         .mode = SpiMode::Mode0,
         .lsbFirst = false,
         .dataSize = SpiDataSize::Bits8}
    )
};
Uart<> g_uart{
    *USART2,
    USART2_IRQn,
    uart(
        {.baudrate = 115200,
         .dataBits = DataBits::Eight,
         .stopBits = StopBits::One,
         .parity = Parity::None}
    )
};
```

The validator and enums come from the interface module (`driver.i2c` /
`driver.spi` / `driver.uart`) — `import` it alongside the `driver.stm32f4.*`
implementation (the impl does not re-export it), same as `import driver.gpio`
next to `import driver.stm32f4.gpio`.

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
Uart<512, 256> uart{...};                    // interrupt mode, default
Uart<512, 256, UartMode::Dma> uartDma{...};  // DMA TX
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
}  // namespace
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

## System / Component framework (v0.1.8)

Optional layer under `sdk/system/`, enabled by `STM32_USE_SYSTEM` (requires
`STM32_USE_DRIVERS`), linked as `stm32_system`. Mirrors the v0.1.7 concept
style — **zero vtable, no heap**. Full guide: `docs/modules/system.md`.

- **`Component` is a concept, not a virtual base.** A component models it by
  providing `onRegister/onInit/onBind/onStart` (each returns `driver::Status`)
  plus state/criticality/name accessors — the accessors come free from the
  non-virtual `system::ComponentBase` mix-in. Hooks are `on*`-prefixed because
  `register` is a C++ keyword. End every component with
  `static_assert(system::Component<Foo>)`.
- **DI via `Config` + `Environment`.** `Config` = compile-time constants
  (embed `system::ComponentConfig base` for name + criticality); `Environment` =
  references to dependencies. A component needing a generic bus is a constrained
  template (`template <driver::II2c I2cDriver>`), same terse style as sensors.
- **Composition root is a `struct`.** Members = the dependency graph, built in
  declaration order; later members reference earlier ones by ref. The single
  `system::bootstrap(a, b, c)` argument list is the one source of truth — no
  manual registration list.
- **`bootstrap` is a variadic fold** (no `<tuple>`, no runtime registry). It
  runs the four phases as a barrier and returns a `BootReport`. `Criticality`:
  `Critical` failure aborts; `Common` failure is counted (`degraded`) and the
  component skipped in later phases while the system continues.
- FreeRTOS tasks created before `startScheduler()` don't run until it starts, so
  `boot()` (the four hooks) and task creation both happen in `main` pre-scheduler.
  Task entry is a static trampoline; cast `void*` with `decltype(&app.member)`.

### Concurrency layer (v0.1.9)

Three layered modules in the same `stm32_system` lib, same zero-vtable/zero-heap
style. Callables are `void(*)(void*)` thunks (never `std::function`). Full guide:
`docs/modules/system.md#concurrency-layer-v019`.

- **`WorkItem` / `WorkQueue` (`system.work_queue`)** — intrusive, client-owned
  deferred work; **no heap, no RTOS** (CMSIS PRIMASK critical section behind
  `system::detail::enterCritical/leaveCritical`, the only TU-local CMSIS touch
  point). `WorkItem::bind<&T::method>(obj)` is the captureless factory. Time is
  injected: `runDue(now)` for ticks, `runOnce()` for super-loops. Scheduling is
  **idempotent** (`_queued` guard) — this is what makes a channel coalesce, not
  corrupt the list. Ordering has a `consteval` self-check (like `resultSelfCheck`).
- **`SingleThreadExecutor` (`system.executor`, FreeRTOS only)** — a `WorkQueue`
  bound to one `rtos::Task` + wake semaphore; `_task` is the LAST member (ctor
  captures `this` after `_wq`/`_wake` exist). Handlers run serially on one task —
  no per-handler mutex. `post/postAfter/addPeriodic/postFromISR`; periodic
  re-arm is core-driven (`WorkItem::setPeriodTicks`).
- **`Channel<Event, MaxSubs>` (`system.signal_bus`, FreeRTOS only)** — type-safe
  pub/sub: `Event` is a trivially-copyable tag, subscribers in a fixed array.
  `publish` stores under a critical section + posts the channel's `WorkItem` to
  the executor; `dispatch()` fans out with an **index loop** (never range-for /
  span across the module boundary — the GCC-15 gotcha). Diverges from the
  reference on purpose: no string publisher names, no `reinterpret_cast`.
- Demo: `freertos/signal-bus-demo` (a `Producer` with a task publishes; a
  reactive `Consumer` with no task subscribes in `onBind()`).

### Concurrency layer completion (v0.1.10)

Finishes the layer (issues #52–#55). Same zero-vtable/zero-heap style.

- **EXTI driver (`driver.stm32f4.exti` + concept `driver.exti`)** — routes a GPIO
  pin to an EXTI line. `ExtiLine` is **non-template**, so it calls CMSIS
  (`NVIC_*`, `__DSB`) directly (no `detail::` wrappers, unlike `Uart<>`). All MMIO
  through `reg::*`; `EXTI->PR` is write-1-to-clear via `reg::write`. Callback is a
  captureless thunk (`ExtiLine::bind<&T::method>(cfg, obj)`). ISR pattern mirrors
  UART: `EXTI0_IRQHandler() { obj.irqHandler(); }`; shared vectors (5–9, 10–15)
  call `irqHandler()` on every line object. `exti({...})` is a `consteval`
  validator like `gpio()`.
- **`Timer` (`system.timer`, FreeRTOS only)** — thin wrapper over the executor
  (`postAfter`/`addPeriodic`/`cancel`); owns one `WorkItem`, thunk callback.
  `start`/`startPeriodic`/`stop`. `start` cancels + zeroes the period first, so
  re-arm is always defined.
- **`Channel<Event, MaxSubs, RingDepth = 1>`** — added trailing `RingDepth`.
  Default 1 = the v0.1.9 coalescing slot; >1 = FIFO ring, drop-oldest. Unified
  via `detail::EventRing<Event, Depth>` (drop-oldest at depth 1 *is* coalescing) —
  its ordering has a `consteval` self-check (`eventRingSelfCheck`).
- **Deferred-start `rtos::Task`** — default ctor + idempotent `create(...)`. A
  component owns `rtos::Task _task;` and starts it in `onStart()` (still
  pre-scheduler). Task ownership moves out of `main`.
- ISR that calls `postFromISR` must have priority numerically ≥
  `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 on F4) — the EXTI demo uses 6.
- Demo: `freertos/button-events-demo` (EXTI button → `postFromISR` → debounce
  `Timer` → ring `Channel` → LED, plus a `Heartbeat` component owning its task).

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
