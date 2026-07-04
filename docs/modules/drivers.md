# Drivers

All drivers live under `sdk/drivers/include/driver/`. Each peripheral has a
C++20 **concept** in `interface/i_*.cppm` (the compile-time contract) plus a
concrete STM32F4 implementation in `stm32f4/*.cppm` that models it.

Since v0.1.7 the interfaces are **concepts, not virtual base classes**
(`IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash`). A driver simply provides the
required methods — no inheritance, no vtable on bus calls. Code that needs a
generic peripheral takes it as a constrained template parameter
(`template <driver::II2c I2cDriver>`), and a test double is just a plain `struct`
that satisfies the concept. Each implementation ends with a
`static_assert(IXxx<Impl>)` self-check so a signature drift is a compile
error.

## Rules

- Configs **have no defaults** — every field must be set explicitly.
- MMIO access must go through `driver::reg::set/clear/write/read/get/modify`.
  Raw `|=`, `&=`, or assignment on `volatile uint32_t` registers (`RCC->...`,
  peripheral `CR1`, DMA stream `NDTR`, etc.) is forbidden.
- Globals over static-locals or smart pointers; peripherals live for the
  lifetime of the program.
- No raw pointers to peripherals — use references (`GPIO_TypeDef&`).

## Error handling — `driver.types` (v0.1.5)

Drivers report failures through `enum class Status : uint8_t` (`Ok`, `None`,
`Timeout`, `Nack`, `BusError`, `Busy`, `InvalidArg`, `HardwareError`). Since
v0.1.5 `Status` is `[[nodiscard]]`: every `Status`-returning call must be
consumed, or the discard made explicit with `(void)`. With `-Werror` an
ignored `Status` is a build error. Since v0.1.7 **every value-returning driver
and sensor method** is `[[nodiscard]]` too — including byte counts
(`Uart::write`/`read`) and geometry (`Flash::sectorSize`, `Display::width`, …),
so a fire-and-forget UART write needs an explicit `(void)`.

```cpp
import driver.types;

if (g_i2c.write(addr, payload) != driver::Status::Ok) { /* handle */
}

(void) _rxBuf.push(byte);  // intentional drop inside an ISR
```

### `Result<T>`

`Result<T>` carries **either** a value of `T` **or** an error `Status`
(1-byte tag, no allocation, no exceptions). It is `[[nodiscard]]` and fully
`constexpr`. `T` must be trivially destructible (true for the POD payloads
drivers return).

```cpp
import driver.types;
using driver::Result;
using driver::Status;

Result<uint16_t> r = readAdc();  // value or error
if (r.ok()) {
  uint16_t v = r.value();  // precondition: ok() — UB otherwise
}
uint16_t safe = r.valueOr(0);  // never UB
Status st = r.status();        // Ok on success, the error otherwise
```

`value()` on an error is undefined behaviour (like `*std::optional`); use
`valueOr()` or check `ok()` first. Passing `Status::Ok` as an error is
normalised to `None`, so an error `Result` never reports `ok()`.

### `DRV_TRY` / `DRV_TRY_ASSIGN` — early return

Macros cannot be exported by a C++20 module, so the Rust-`?`-style helpers
live in the textual header `driver/try.hpp`. Include it **after**
`import driver.types;`; the enclosing function must return `Status` (or
`Result<U>`).

```cpp
import driver.types;
#include "driver/try.hpp"

driver::Status configure() {
  DRV_TRY(g_i2c.writeReg(addr, REG_CTRL, ctrl));  // return Status if not Ok

  uint16_t raw = 0;
  DRV_TRY_ASSIGN(raw, readAdc());  // unwrap Result or return err
  use(raw);
  return driver::Status::Ok;
}
```

## GPIO — `driver.stm32f4.gpio`

```cpp
import driver.stm32f4.gpio;
using driver::gpio;
using driver::OutputSpeed;
using driver::OutputType;
using driver::PinMode;
using driver::PullMode;
using driver::stm32f4::GpioPin;

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

g_led.write(true);
g_led.toggle();
```

The `gpio({...})` free function is a `consteval` validator: it throws
(at compile time) on invalid values like `pin > 15`, `mode == None`, missing
`speed`/`type` for Output / AlternateFunction modes, or `af > 15` for AF mode.

GPIO port clock enable is handled by `GpioPin`'s constructor (it inspects the
port address and toggles the right `RCC_AHB1ENR_GPIOxEN` bit).

### `NullGpioPin` — `driver.null_gpio` (v0.1.4)

A no-op `struct` that models the `IGpioPin` concept. Use it when a sensor
takes a CS pin that is **hardwired** on the board (for example, an SPI flash
whose CS is tied to GND through a jumper).

```cpp
import driver.null_gpio;
import driver.stm32f4.spi;

driver::NullGpioPin null_cs;
// W25q32 is templated on its bus + CS types; CTAD deduces them from the args:
sensor::W25q32 flash{
    spi,
    null_cs,
    {.expectedJedecId = sensor::W25q32Spec::JEDEC_W25Q32JV,
     .busyPollLoops = 5000000U}
};  // hardware CS — no toggling
```

The four pin methods (`set`, `reset`, `toggle`, `read`) are inline empty
functions. Since v0.1.7 `NullGpioPin` is a plain `struct` (not a subclass of a
virtual base), so there is no vtable at all — the calls compile to nothing.
There is no register access, no clock enable, and no peripheral state. It is
intentionally chip-agnostic and lives at the top of
`sdk/drivers/include/driver/null_gpio.cppm` rather than under `stm32f4/`.

## I2C — `driver.stm32f4.i2c`

```cpp
import driver.i2c; // I2cConfig + the i2c() validator
import driver.stm32f4.i2c;
using driver::i2c;
using driver::stm32f4::I2c;

I2c g_i2c1{
    *I2C1,
    i2c({
        .clockSpeed = 400000,
        .fastMode = true,
    }),
};
```

`i2c({...})` is a `consteval` validator (like `gpio()` / `exti()`): it throws at
compile time on `clockSpeed` outside `[1, 400000]`, or `clockSpeed > 100000`
without `fastMode`. `I2cConfig` and `i2c()` live in the interface module
`driver.i2c` — import it alongside `driver.stm32f4.i2c` (the implementation does
not re-export it).

Methods (required by the `II2c` concept):

- `Status write(uint8_t addr, std::span<const uint8_t> data)`
- `Status read(uint8_t addr, std::span<uint8_t> data)`
- `Status writeReg(uint8_t addr, uint8_t reg, std::span<const uint8_t> data)`
- `Status readReg(uint8_t addr, uint8_t reg, std::span<uint8_t> data)`
- `Status probe(uint8_t addr)`

Multi-byte read uses the N=1 / N=2 / N≥3 closing-sequence from RM0090 §27.3.3.
The driver enters critical sections (`taskENTER_CRITICAL` under FreeRTOS,
`__disable_irq` otherwise) around the BTF → STOP → DR-read windows so an ISR
cannot disrupt the close. `sendAddress()` races `ADDR` against `AF` so a NACK
aborts almost immediately — bus scans complete in ≈ 200 ms for the 0x03..0x77
range.

Clock enable: caller's responsibility, typically from `__initialize_hardware()`:

```cpp
driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);
```

## UART — `driver.stm32f4.uart`

```cpp
import driver.uart; // UartConfig, DataBits/StopBits/Parity, uart()
import driver.stm32f4.uart;
using driver::DataBits;
using driver::Parity;
using driver::StopBits;
using driver::uart;
using driver::stm32f4::Uart;
using driver::stm32f4::UartMode;

// Interrupt mode (default)
Uart<512, 256> g_uart2{
    *USART2,
    USART2_IRQn,
    uart({
        .baudrate = 115200,
        .dataBits = DataBits::Eight,
        .stopBits = StopBits::One,
        .parity = Parity::None,
    }),
};

extern "C" void USART2_IRQHandler() {
  g_uart2.irqHandler();
}
```

`uart({...})` is a `consteval` validator: it throws at compile time on
`baudrate == 0` or an unset `dataBits`/`stopBits` (`DataBits::None` /
`StopBits::None`). `UartConfig` and the enums live in `driver.uart`.

Template parameters: `Uart<RxBufSize, TxBufSize, Mode>` where:

- `RxBufSize`, `TxBufSize` — power-of-2, minimum 16 (enforced by `static_assert`).
- `Mode` — `UartMode::Interrupt` (default) or `UartMode::Dma` (TX via DMA).

DMA mode requires extra ctor arguments and DMA ISR registration:

```cpp
Uart<512, 256, UartMode::Dma> g_uart2{
    *USART2,
    USART2_IRQn,
    driver::stm32f4::dmaMap::usart2_tx,
    uart(
        {.baudrate = 115200,
         .dataBits = DataBits::Eight,
         .stopBits = StopBits::One,
         .parity = Parity::None}
    ),
};

extern "C" void DMA1_Stream6_IRQHandler() {
  g_uart2.dmaTxIrqHandler();
}
```

DMA TX issues one transfer-complete IRQ per `write()` call (vs one IRQ per byte
in interrupt mode). For a 128-byte string at 115200 baud, CPU time spent in ISR
drops from ~11 ms to under 50 µs.

## SPI — `driver.stm32f4.spi`

```cpp
import driver.spi; // SpiConfig, SpiMode/SpiDataSize, spi()
import driver.stm32f4.spi;
using driver::spi;
using driver::SpiDataSize;
using driver::SpiMode;
using driver::stm32f4::Spi;

Spi g_spi2{
    *SPI2,
    spi({
        .clockHz = 10'000'000,
        .mode = SpiMode::Mode0,
        .lsbFirst = false,
        .dataSize = SpiDataSize::Bits8,
    }),
};
```

`spi({...})` is a `consteval` validator: it throws at compile time on
`clockHz == 0` or an unset `mode`/`dataSize` (`SpiMode::None` /
`SpiDataSize::None`). `SpiConfig`, `SpiMode` (`Mode0..Mode3`, CPOL/CPHA) and
`SpiDataSize` (`Bits8`/`Bits16`) live in `driver.spi`.

`Spi` selects `PCLK1` for SPI2/SPI3 (APB1) and `PCLK2` for SPI1/SPI4/SPI5/SPI6
(APB2) when computing the BR divisor.

DMA support for SPI is not yet implemented; the stream/channel mapping is
reserved in `driver.stm32f4.dma::dmaMap` for a future PR.

## DMA — `driver.stm32f4.dma`

```cpp
import driver.stm32f4.dma;
using driver::stm32f4::DmaConfig;
using driver::stm32f4::DmaDir;
using driver::stm32f4::DmaPrio;
using driver::stm32f4::DmaStream;

DmaStream txDma{
    driver::stm32f4::dmaMap::usart2_tx,
    {
        .dir = DmaDir::MemToPeriph,
        .mode = DmaMode::Normal,
        .priority = DmaPrio::Medium,
        .memInc = true,
        .periphInc = false,
        .memDataSize = 1,
        .periphDataSize = 1,
    },
};
```

`DmaStream` is an RAII wrapper. The peripheral ↔ stream/channel mapping for
F407VG lives in the `dmaMap` namespace:

| Peripheral | Stream | Channel |
|------------|--------|---------|
| USART1 TX | DMA2/Stream 7 | 4 |
| USART1 RX | DMA2/Stream 5 | 4 |
| USART2 TX | DMA1/Stream 6 | 4 |
| USART2 RX | DMA1/Stream 5 | 4 |
| SPI1 TX | DMA2/Stream 3 | 3 |
| SPI1 RX | DMA2/Stream 0 | 3 |
| SPI2 TX | DMA1/Stream 4 | 0 |
| SPI2 RX | DMA1/Stream 3 | 0 |

## Internal flash — `driver.stm32f4.internal_flash`

```cpp
import driver.stm32f4.internal_flash;
using driver::stm32f4::InternalFlash;

InternalFlash g_flash;
g_flash.eraseSector(11);
g_flash.write(0x080E0000, std::span{data});
```

Sector layout follows the chip: F407VG has 12 sectors (16K×4, 64K×1, 128K×7).
`InternalFlash` enables the flash controller clock and handles unlock /
program / lock sequences.

## EXTI — `driver.stm32f4.exti` (v0.1.10)

External-interrupt line driver: routes a GPIO pin to an EXTI line, configures
the edge, and dispatches to a captureless callback from the ISR. The concept
`driver::IExti` and the `ExtiConfig` aggregate + `exti({...})` validator live in
`driver.exti`; the F4 implementation `ExtiLine` in `driver.stm32f4.exti`.

```cpp
import driver.exti;
import driver.stm32f4.exti;
using driver::exti;
using driver::ExtiPort;
using driver::ExtiTrigger;
using driver::stm32f4::ExtiLine;

// Bind the line to a member; the callback runs in interrupt context.
ExtiLine g_button = ExtiLine::bind<&App::onButton>(
    exti({
        .line = 0,
        .port = ExtiPort::A,
        .trigger = ExtiTrigger::Rising,
        .priority = 6,
    }),
    app
);

extern "C" void EXTI0_IRQHandler() {
  g_button.irqHandler();
}
```

- `exti({...})` is a `consteval` validator: it throws (at compile time) on
  `line > 15` or `priority > 15` (F4 has 4 NVIC priority bits).
- The constructor enables the SYSCFG clock, selects the port in
  `SYSCFG_EXTICR`, sets `RTSR`/`FTSR` for the edge, clears any pending bit, sets
  the NVIC priority, enables the IRQ and unmasks `IMR`. `ExtiPort` values map
  directly onto the 4-bit `EXTICR` nibble (A = 0 … H = 7).
- `irqHandler()` checks this line's `PR` bit, clears it (write-1-to-clear) and
  calls the bound callback. Lines 0–4 have a dedicated IRQ; 5–9 share
  `EXTI9_5_IRQn` and 10–15 share `EXTI15_10_IRQn`. On a shared vector call
  `irqHandler()` on **every** line object that uses it — each checks its own
  pending bit.
- `enable()` / `disable()` toggle the mask; `pending()` reads `PR`;
  `clearPending()` clears it manually.

The GPIO pin itself is configured separately with `GpioPin` (input mode). To
defer work out of the ISR, have the callback call
`executor.postFromISR(workItem)` — the ISR priority (6 here) must be numerically
≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 on F4) for that to be legal.
See the `button-events-demo` template.
