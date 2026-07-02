# Upgrade notes

This page lists what to change in your project's source when you bump the SDK
version. The project is still pre-1.0 — public APIs may be refined between
releases. Pin a specific tag in `stmproject.toml` (instead of `develop`) and
upgrade deliberately.

## Recommended workflow

1. On a feature branch of your project, bump `[sdk] version` to the new tag.
2. Run `stmtool sdk update --version <tag>`.
3. Re-run formatter: `clang-format -i src/**/*.cpp src/**/*.cppm`.
4. Try `stmtool build --native --clean`. Address compiler errors one by one.
5. Flash to hardware and verify the smoke-test for your scenario.
6. Merge back once green.

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
GpioPin led{*GPIOD, GpioConfig{12, PinMode::Output, PullMode::None,
                               OutputSpeed::Low, OutputType::PushPull}};
```

**After (latest):**

```cpp
GpioPin led{*GPIOD, gpio({.pin = 12, .mode = PinMode::Output,
                          .pull = PullMode::None, .speed = OutputSpeed::Low,
                          .type = OutputType::PushPull})};
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
Uart<512, 256, UartMode::Dma> g_uart2{ ... };
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

`.clang-format` adds:

- `BinPackArguments: false`, `BinPackParameters: false`
- `AllowAllArgumentsOnNextLine: false`, `AllowAllParametersOfDeclarationOnNextLine: false`
- `AlignAfterOpenBracket: BlockIndent`
- `Cpp11BracedListStyle: false`

If you run `clang-format -i` after upgrading, expect noisy diffs on call sites
that pass aggregate configs. Use trailing commas after the last designated
initializer to preserve multi-line formatting deterministically.

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
