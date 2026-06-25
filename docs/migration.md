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

## Upgrading to v0.1.5

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
