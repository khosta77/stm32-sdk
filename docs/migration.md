# Upgrade notes

This page lists what to change in your project's source when you bump the SDK
version. The project is still pre-1.0 — public APIs may be refined between
releases. Pin a specific tag in `stmproject.toml` (instead of `develop`) and
upgrade deliberately.

## Recommended workflow

1. On a feature branch of your project, bump `[sdk] version` to the new tag.
2. Run `stmtool sdk update --version <tag>`.
3. Re-run formatter: `clang-format -i src/**/*.cpp src/**/*.cppm`.
4. Try `stmtool build --clean`. Address compiler errors one by one.
5. Flash to hardware and verify the smoke-test for your scenario.
6. Merge back once green.

## New in v0.2.1

v0.2.1 moves `stmtool` out of the SDK monorepo into its own repository,
[`khosta77/stmtool`](https://github.com/khosta77/stmtool). There are **no
source-visible SDK changes** — only how you install and update the tool.

### Action: reinstall `stmtool` from its new repository

`pip install ./tools/stmtool` no longer works (the directory is gone). Reinstall
from the standalone repo:

```bash
pipx uninstall stmtool 2>/dev/null || true
pipx install git+https://github.com/khosta77/stmtool.git
```

`./install.sh` in the SDK still works and now points at the new repository. The
CLI, commands, and `STMSDK_PATH` / `STMTOOL_DOCKER_IMAGE` behavior are unchanged;
the tool resolves this SDK checkout exactly as before. `stmtool` now versions
itself as `v0.N` independently of the SDK release tags. The version command is
`stmtool show-version`.

## New in v0.2.0

v0.2.0 is a cross-cutting stabilization release: a driver/sensor/framework bug
audit, closing accumulated debt, and routing every peripheral through the
`reg::` helpers. Most changes are internal, but three are **source-visible** for
downstream projects.

### Breaking: `SpiDataSize::Bits16` is now rejected at compile time

The STM32F4 SPI driver only ever drove the data register one byte at a time, so
`SpiDataSize::Bits16` produced corrupt framing. `spi({...})` now rejects it with
a compile-time error. If you passed `Bits16`, switch to `Bits8`:

```cpp
Spi g_spi{*SPI2, spi({.clockHz = 10000000,
                      .mode = SpiMode::Mode0,
                      .lsbFirst = false,
                      .dataSize = SpiDataSize::Bits8})};  // was Bits16
```

### Breaking: more methods are `[[nodiscard]]`

Every value-returning driver/RTOS/util method now carries `[[nodiscard]]`
(`CircularBuffer::{read,write,size,free_space,empty,full,capacity}`,
`reg::{read,get}`, the `DmaStream` status getters, `rtos::{Mutex,BinarySemaphore,
Queue,Task}` methods, `clock::get*`). Under the SDK's `-Werror`, ignoring one of
these returns now fails to compile. Consume the value or mark the discard
explicit:

```cpp
(void) uart.write(data);  // fire-and-forget is now explicit
```

### Breaking: `IImu` range setters return `driver::Status`

`setAccelRange` / `setGyroRange` changed from `void` to `driver::Status` so a
bus NACK during `init()` is no longer swallowed. The `sensor::IImu` concept was
updated to match. Custom IMU implementations must update both method signatures;
the `static_assert(IImu<...>)` in your driver enforces it at compile time.

### Non-breaking fixes worth knowing

- **GPIO clock** is now enabled for ports F–I (previously only A–E), so pins on
  those ports work on 100/144/176-pin parts.
- **`postFromISR`** now yields to a woken higher-priority task instead of waiting
  up to a tick — lower ISR-to-task latency for the executor and signal bus.
- **Peripheral busy-waits** (DMA enable, UART DMA transfer/TC) are now bounded
  and return `Status::Timeout` instead of spinning forever on a stuck peripheral.
- **UART DMA `writeNonBlocking`** now actually returns immediately instead of
  blocking for the whole transfer.
- All MMIO in the drivers now goes through `driver::reg::*`; behaviour is
  byte-identical.

> Known gap: `system.work_queue` and `system.signal_bus` still pull in CMSIS /
> FreeRTOS, so they are not yet covered by host tests. Extracting a portable
> critical-section shim is tracked for a later release.

## New in v0.1.16

v0.1.16 completes the C→C++ migration started in v0.1.15: the newlib glue and
the CMSIS `system_*` / vector-table sources are now C++. **No breaking changes,
nothing to do on upgrade** — symbol names, linkage and behaviour are
byte-for-byte identical.

- **Nothing to change.** `core/src/newlib/*.c`, `system_stm32f4xx.c` and the
  per-family `vectors_*.c` became `.cpp`, but `_start`, `_sbrk`, `_exit`,
  `SystemInit`, `Default_Handler`, every IRQ handler and the `__isr_vectors`
  table keep their `extern "C"` name and linkage.
- **Weak overrides still work.** Redefining `_sbrk`, `_write`, a weak IRQ
  handler, `SystemInit` or `__assert_func` from your application (`.cpp` or
  `.c`) overrides the SDK default exactly as before.
- **Vendor headers stay C headers.** The CMSIS device headers (`stm32f4xx.h`
  and the family headers it pulls in) remain `.h` and pristine so they stay
  diffable against ST upstream; only the `.c` runtime sources moved to `.cpp`.

## New in v0.1.15

v0.1.15 migrates the SDK's own low-level runtime glue from C to idiomatic C++.
**No breaking changes, nothing to do on upgrade** — symbol names, linkage and
behaviour are byte-for-byte identical.

- **Nothing to change.** `trace.c`/`trace-impl.c`, `initialize-hardware.c`,
  `reset-hardware.c`, `exception-handlers.c` and `freertos_hooks.c` became
  `.cpp`, but every symbol the vector table, newlib startup and the FreeRTOS
  kernel reference stays `extern "C"` with the same name.
- **Overrides still work.** If your application redefines the weak
  `__initialize_hardware`, `__initialize_hardware_early`, `__reset_hardware`, an
  exception handler, or a `vApplication*` hook — from a `.cpp` or a `.c` file —
  it keeps overriding the SDK default exactly as before.
- **Vendor code is untouched.** The CMSIS `system_*` files, the per-family
  vector tables and the newlib runtime remain C and pristine.

## New in v0.1.14

v0.1.14 adds logging and a multi-arch build image. **No breaking changes** —
everything below is opt-in.

- **Logging facility.** `import driver.log;` plus `#include "driver/log.hpp"`
  give the `LOG_*` macros; install a backend (`driver::log::ItmBackend` or
  `driver::log::UartBackend<UartDriver>`) once at startup. Tune it with the
  `STM32_LOG_LEVEL` and `STM32_LOG_BACKEND` cache variables. Defaults
  (`INFO` / `none`) emit nothing until you install a backend, so existing
  projects are unaffected. See [Logging](modules/logging.md).
- **Apple Silicon: `docker pull` once.** The build image is now multi-arch
  (`linux/amd64,linux/arm64`). If you have an amd64 image cached from v0.1.13,
  pull again to get the arm64 layer with a native toolchain — `stmtool build`
  and `stmtool test` then run without the Rosetta crash. No flags or CLI change.

## New in v0.1.13

v0.1.13 is a **quality / infrastructure** release. There are no SDK API changes,
but the build workflow changes in a breaking way:

- **`stmtool build --native` is removed.** All builds now run inside the SDK
  Docker image; there is no host-toolchain path. Drop `--native` from any
  scripts. Docker is now required. If you relied on a local `arm-none-eabi-gcc`,
  install Docker instead — the image ships the pinned toolchain (15.2).
- **Artifacts move from `build/` to `out/`.** `stmtool build` configures into
  `out/`, `stmtool flash` reads the `.bin` from `out/`, and `--clean` wipes
  `out/`. Add `out/` to your project `.gitignore` (SDK templates already do). Any
  CI or scripts that referenced `build/*.elf` should use `out/*.elf`.
- **`stmtool test`** is new: it builds and runs the SDK host unit tests in the
  image. Reusable mock buses (`testing::MockI2c` / `MockSpi` / `MockUart` /
  `MockGpioPin` / `MockFlash`, module `testing.mock`) let you unit-test
  driver/sensor logic on the host — see the testing module docs.
- **GCC >= 14 is enforced.** `stm32_sdk.cmake` fails configuration if the ARM
  compiler is older than GCC 14 (C++20 module scanning needs it). Inside the
  image this is always satisfied; `stmtool doctor` flags a stale local toolchain.
- **FreeRTOS-Kernel is baked into the image** and no longer re-cloned per build,
  so builds are offline. Outside Docker, `stm32_rtos.cmake` still honours
  `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL` for an offline checkout.

## New in v0.1.12

v0.1.12 is a **formatting and tooling** release — no source-behaviour or API
changes. There is nothing to change in your project's C++ source. Two things
affect your local workflow:

- **The SDK style is now Google 2-space / 80-column** (`Cpp11BracedListStyle:
  true`). If your project reuses the SDK `.clang-format`, re-run `clang-format -i`
  and expect a whitespace-only reflow. Vendor / third-party files you keep in
  your project can be excluded with your own `.clang-format-ignore` (needs
  clang-format 18+).
- **Optional pre-commit hooks.** The SDK now ships `.pre-commit-config.yaml`
  (clang-format pinned to 19.1.3 + Conventional Commits + `poe ci`). To adopt the
  same gates in your project:

  ```bash
  pip install pre-commit
  pre-commit install --hook-type pre-commit --hook-type commit-msg
  ```

  Use clang-format 19 to match the pinned version; older majors may reflow
  differently.

## New in v0.1.11

v0.1.11 introduces compile-time validation for the I2C / SPI / UART configs.
This is a **breaking change** for any code that constructs an `I2c`, `Spi`, or
`Uart<>`. The thread-safety (#57) and unit-test (#58) additions are purely
additive — new headers, nothing to change in existing code.

### Breaking: I2C / SPI / UART configs (#56)

Three things changed for these drivers:

1. **Wrap the config in its validator.** The config must now go through the free
   `consteval` validator `i2c()` / `spi()` / `uart()`, exactly like `gpio()` /
   `exti()`. An invalid config is now a compile error instead of silently wrong.
2. **SPI / UART fields are now strong enums**, not bare integers: `.mode`,
   `.dataSize`, `.dataBits`, `.stopBits`.
3. **The config type moved out of the implementation.** `I2c::Config` is now the
   free `driver::I2cConfig` (and likewise `driver::SpiConfig` /
   `driver::UartConfig`), living in the interface module. The implementation no
   longer re-exports the interface, so add `import driver.i2c;` (resp.
   `driver.spi` / `driver.uart`) next to `import driver.stm32f4.i2c;`.

Before:

```cpp
import driver.stm32f4.i2c;
import driver.stm32f4.spi;
import driver.stm32f4.uart;

I2c g_i2c1{*I2C1, {.clockSpeed = 400000, .fastMode = true}};
Spi g_spi1{*SPI1, {.mode = 0, .dataSize = 8, ...}};
Uart<> g_uart2{
    *USART2,
    USART2_IRQn,
    {.baudrate = 115200, .dataBits = 8, .stopBits = 1, .parity = Parity::None}
};
```

After:

```cpp
import driver.i2c;
import driver.spi;
import driver.uart;
import driver.stm32f4.i2c;
import driver.stm32f4.spi;
import driver.stm32f4.uart;

I2c g_i2c1{*I2C1, i2c({.clockSpeed = 400000, .fastMode = true})};
Spi g_spi1{
    *SPI1,
    spi({.mode = SpiMode::Mode0, .dataSize = SpiDataSize::Bits8, ...})
};
Uart<> g_uart2{
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

Field mapping for the enum conversion:

- `.mode = 0` → `.mode = SpiMode::Mode0` (likewise `Mode1`..`Mode3`).
- `.dataSize = 8` → `.dataSize = SpiDataSize::Bits8` (`16` → `Bits16`).
- `.dataBits = 8` → `.dataBits = DataBits::Eight` (`9` → `Nine`).
- `.stopBits = 1` → `.stopBits = StopBits::One` (`2` → `Two`).

### Additive: thread-safety and unit tests (#57, #58)

Nothing to change. `util/thread_safety.hpp` (in `sdk/core/include/util/`) is a
pure-preprocessor header — its macros are no-ops under GCC and don't affect
codegen. `testing/unit_test.hpp` is a header-only helper behind
`STM32_USE_TESTING` (INTERFACE target `stm32_testing`); opt in only if you want
on-device unit tests. See the new `bare-metal/unit-test-demo` template.

## New in v0.1.10

v0.1.10 completes the concurrency layer. It is **additive** — no edits are
required to upgrade, and every existing template is unchanged apart from the new
`button-events-demo`.

New surface, all opt-in:

1. **EXTI driver** (`STM32_USE_DRIVERS`): `import driver.exti; import
   driver.stm32f4.exti;`. Configure a line with `exti({.line, .port, .trigger,
   .priority})`, bind a callback with `ExtiLine::bind<&T::method>(cfg, obj)`, and
   forward the vector: `extern "C" void EXTI0_IRQHandler() { obj.irqHandler(); }`.
   For lines 5–9 / 10–15 (shared vectors) call `irqHandler()` on each line
   object. If the callback uses `postFromISR`, the ISR priority must be
   numerically ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 on F4).
2. **`system::Timer`** (`STM32_USE_SYSTEM` + FreeRTOS): `import system.timer;`.
   `Timer::bind<&T::method>(exec, obj)`, then `start(ms)` / `startPeriodic(ms)` /
   `stop()`.
3. **Ring channels**: `system::Channel` now takes an optional third template
   argument `RingDepth` (default `1`). Existing `Channel<Event, MaxSubs>` code is
   unchanged; use `Channel<Event, MaxSubs, N>` when you must not coalesce events.
4. **Component-owned tasks**: `rtos::Task` is now default-constructible with an
   idempotent `create(...)`. Move task creation from `main` into a component's
   `onStart()` if you want the component to own its worker.

No existing symbol changed signature in a breaking way; the `Channel` and
`rtos::Task` additions are backward compatible. See
[System](modules/system.md#concurrency-layer-additions-v0110) and
[Drivers](modules/drivers.md#exti--driverstm32f4exti-v0110).

## New in v0.1.9

v0.1.9 adds the optional concurrency layer to `sdk/system/` — `WorkQueue`,
`SingleThreadExecutor`, and a type-safe signal bus. It is **additive**: nothing
in your existing project breaks, and no edits are required to upgrade. Every
in-tree template is byte-for-byte unchanged; the new `signal-bus-demo` template
is the worked example.

If you want to adopt it in your own project (requires `STM32_USE_SYSTEM` +
FreeRTOS):

1. `import system.work_queue;` for the RTOS-free queue, and/or
   `import system.executor; import system.signal_bus;` for the FreeRTOS layers.
   No CMake change beyond what `STM32_USE_SYSTEM` already gives you — the
   executor and signal bus are compiled automatically when FreeRTOS is on.
2. Give each component that defers work an intrusive `system::WorkItem` member
   (`WorkItem::bind<&T::method>(*this)`) and post it to a shared
   `system::SingleThreadExecutor`.
3. Decouple components with `system::Channel<Event, MaxSubs>`: publishers call
   `publish`, subscribers call `subscribe<&T::handler>(*this)` (typically in
   `onBind()`). See [System](modules/system.md#concurrency-layer-v019).

Nothing here changes the v0.1.8 component API — the concurrency layer sits
beside it. If you do not import the new modules, your build is unaffected.

## New in v0.1.8

v0.1.8 adds the optional `sdk/system/` component framework. It is **additive**:
nothing in your existing project breaks, and no edits are required to upgrade.
Every in-tree template except `imu-flash-oled-demo` is byte-for-byte unchanged.

If you want to adopt the framework in your own project:

1. Enable it in `CMakeLists.txt`: `set(STM32_USE_SYSTEM ON ...)` (requires
   `STM32_USE_DRIVERS`), and add `stm32_system` to `target_link_libraries`.
2. `import system.component;` / `import system.bootstrap;`.
3. Model `system::Component` — inherit `system::ComponentBase` and implement
   `onRegister/onInit/onBind/onStart`; end with
   `static_assert(system::Component<Foo>)`.
4. Assemble the graph in a composition-root `struct` and drive it with
   `system::bootstrap(...)`. See [System](modules/system.md).

The `imu-flash-oled-demo` template was rewritten on the framework. If you had
copied the old free-tasks-plus-globals version of that demo, it still compiles
against the SDK — you are not forced onto components. The new version is the
reference for how the pieces fit together.

## Upgrading to v0.1.7

### Driver and sensor interfaces are now C++20 concepts

The virtual base classes `IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash`
(drivers) and `IImu`, `IDisplay`, `IExternalFlash` (sensors) are now **C++20
concepts**, not classes. Drivers and sensors no longer inherit from them; bus
calls inside sensors are direct (no vtable).

If your project only uses the concrete types (the common case — every in-tree
template did), the only edits you may need are the `W25q32Spec` constant rename
and any `sensor::Xxx*` raw-pointer casts below.

- **You took an interface by reference** (`II2c&`, `ISpi&`, `IGpioPin&`, a
  sensor interface, …). A concept is not a type, so `driver::II2c& bus` no
  longer compiles. Take the concrete type, or make the function/class a
  constrained template:

  ```cpp
  // before
  void useBus(driver::II2c &bus);
  // after — constrained template parameter (the style the SDK uses)
  template <driver::II2c I2cDriver>
  void useBus(I2cDriver &bus);
  ```

- **Sensors are now class templates on their bus type.** Construction is
  unchanged thanks to CTAD: `sensor::Mpu6050 g{i2c, {...}}` still works and
  deduces `Mpu6050<I2c>`. But you can no longer name the bare type — e.g.
  `sensor::Mpu6050 *` is ill-formed. Use `decltype(&g)`:

  ```cpp
  // before
  auto *mpu = static_cast<sensor::Mpu6050 *>(ctx);
  // after
  auto *mpu = static_cast<decltype(&g_mpu)>(ctx);
  ```

- **`W25q32` device constants moved to `sensor::W25q32Spec`.** Since `W25q32`
  is now a template, `sensor::W25q32::SECTOR_SIZE` would need template
  arguments. The constants live in the non-template `sensor::W25q32Spec`:

  ```cpp
  // before
  sensor::W25q32::JEDEC_W25Q32JV
  // after
  sensor::W25q32Spec::JEDEC_W25Q32JV       // also CAPACITY, SECTOR_SIZE, PAGE_SIZE
  ```

- **`NullGpioPin` is now a plain `struct`** (no longer derives a virtual base).
  Usage is unchanged.

### Every value-returning driver/sensor method is now `[[nodiscard]]`

Previously only the `Status` return type carried `[[nodiscard]]`. Now the
methods themselves are marked, including the ones that return a count or a
geometry value (`Uart::write`/`read` → `size_t`, `Flash::sectorSize`/
`sectorCount`, `Display::width`/`height`, `IExternalFlash::capacity`/
`jedecId`, …). Under `-Werror` an ignored result is a build error. The common
case is a fire-and-forget UART write — discard it explicitly:

```cpp
// before
g_uart2.write({data, len});
// after
(void) g_uart2.write({data, len});
```

### `driver::Status` is now `[[nodiscard]]`

`Status` (and the new `Result<T>`) carry the `[[nodiscard]]` attribute. Under
the SDK's mandatory `-Werror`, any call site that **ignored** a `Status`
return now fails to compile. This can surface in downstream code that called
`write` / `read` / `eraseSector` / `init` and dropped the result.

Fix each site one of two ways:

```cpp
// 1. Handle the error (preferred):
if (g_flash.eraseSector(0) != driver::Status::Ok) {
  // log / retry / halt
}

// 2. Explicitly discard, when the failure is genuinely irrelevant
//    (e.g. dropping a byte in a full ISR ring buffer):
(void) _rxBuf.push(byte);
```

`Result<T>`, `DRV_TRY`, and `DRV_TRY_ASSIGN` are new additive APIs — they do
not break existing code. See
[drivers](modules/drivers.md#error-handling-drivertypes-v015).

## Upgrading from v0.1.2

### `GpioConfig` is now an aggregate, validation through `gpio({...})`

The positional `consteval GpioConfig(...)` constructor was removed in favor of
a plain aggregate + a free `consteval gpio()` validator. The new form is
required at every call site.

**Before (v0.1.2):**

```cpp
GpioPin led{
    *GPIOD,
    GpioConfig{
        12,
        PinMode::Output,
        PullMode::None,
        OutputSpeed::Low,
        OutputType::PushPull
    }
};
```

**After (latest):**

```cpp
GpioPin led{
    *GPIOD,
    gpio(
        {.pin = 12,
         .mode = PinMode::Output,
         .pull = PullMode::None,
         .speed = OutputSpeed::Low,
         .type = OutputType::PushPull}
    )
};
```

Trailing comma after the last designated initializer keeps clang-format from
collapsing the config back into a single line.

### GPIO is per-pin

The interface was redesigned from per-port (`IGpio`) to per-pin (`IGpioPin`).
Each `GpioPin` instance owns one configured pin; what used to be port-wide
methods now lives on individual pins.

**Before:**

```cpp
Gpio g_porta{*GPIOA};
g_porta.write(5, true);
```

**After:**

```cpp
GpioPin g_pa5{*GPIOA, gpio({.pin = 5, .mode = PinMode::Output, ...})};
g_pa5.write(true);
```

### `Uart<>` gained a third template parameter (backward compatible)

`Uart<RxBufSize, TxBufSize, Mode>` where `Mode` defaults to
`UartMode::Interrupt`. Existing code that wrote `Uart<256, 256>` continues to
work unchanged. DMA TX is opt-in:

```cpp
Uart<512, 256, UartMode::Dma> g_uart2{...};
```

## New modules available

After upgrading, the following modules are importable without further
configuration changes:

- `driver.stm32f4.dma` — `DmaStream` RAII wrapper + `dmaMap` table.
- `sensor.display` — `IDisplay` interface.
- `sensor.ssd1306` — SSD1306 OLED driver.
- `sensor.external_flash` — `IExternalFlash` interface.
- `sensor.w25q32` — W25Q32 SPI flash driver.

## clang-format changes

As of v0.1.12 the style is Google with these deviations:

- `IndentWidth` / `TabWidth`: 2, `ColumnLimit`: 80, `AccessModifierOffset`: -2
- `BinPackArguments: false`, `BinPackParameters: false`
- `AllowAllArgumentsOnNextLine: false`, `AllowAllParametersOfDeclarationOnNextLine: false`
- `AlignAfterOpenBracket: BlockIndent`
- `Cpp11BracedListStyle: true` (braced-init lists format as `{nullptr}`)

Vendor CMSIS / newlib sources are excluded via `.clang-format-ignore`
(clang-format 18+). If you run `clang-format -i` after upgrading, expect noisy
diffs on call sites that pass aggregate configs. Use trailing commas after the
last designated initializer to preserve multi-line formatting deterministically.

## I2C behaviour

If you read multi-byte buffers from I2C at 400 kHz, the read path is now
spec-compliant per RM0090 §27.3.3. Previously the driver could read garbage in
the last 1-2 bytes of `read()` / `readReg()` at high clock rates; sensors that
read short bursts (e.g. MPU6050's 14-byte block) were the most affected. No
source change required, the fix is transparent.

`I2c::probe()` now races `ADDR` against `AF` for fast NACK detection. Bus scans
that previously took ~58 s now complete in ~200 ms.

## Future-proofing your project

- Keep `[sdk] version` pinned to a tag in `main`/`master`; use `develop` only
  on dedicated experimentation branches.
- Build CI for your project against multiple SDK tags simultaneously if you
  rely on the SDK heavily.
- Subscribe to the GitHub Releases page to catch new tags as they come.
