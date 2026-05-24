# Drivers

All drivers live under `sdk/drivers/include/driver/`. Each peripheral has an
abstract interface in `interface/i_*.cppm` plus a concrete STM32F4
implementation in `stm32f4/*.cppm`.

## Rules

- Configs **have no defaults** — every field must be set explicitly.
- MMIO access must go through `driver::reg::set/clear/write/read/get/modify`.
  Raw `|=`, `&=`, or assignment on `volatile uint32_t` registers (`RCC->...`,
  peripheral `CR1`, DMA stream `NDTR`, etc.) is forbidden.
- Globals over static-locals or smart pointers; peripherals live for the
  lifetime of the program.
- No raw pointers to peripherals — use references (`GPIO_TypeDef&`).

## GPIO — `driver.stm32f4.gpio`

```cpp
import driver.stm32f4.gpio;
using driver::gpio;
using driver::stm32f4::GpioPin;
using driver::PinMode;
using driver::PullMode;
using driver::OutputSpeed;
using driver::OutputType;

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

A no-op implementation of `IGpioPin`. Use it when a sensor or driver
takes an `IGpioPin&` for a CS line that is **hardwired** on the board
(for example, an SPI flash whose CS is tied to GND through a jumper).

```cpp
import driver.null_gpio;
import driver.stm32f4.spi;

driver::NullGpioPin null_cs;
W25q32 flash{spi, null_cs, {.id = 0xEF4016}};  // hardware CS — no toggling
```

All four `IGpioPin` methods (`set`, `reset`, `toggle`, `read`) are
inline empty functions, so the compiler typically devirtualises them
into nothing when the concrete type is known at the call site. There
is no register access, no clock enable, and no peripheral state. It is
intentionally chip-agnostic and lives at the top of
`sdk/drivers/include/driver/null_gpio.cppm` rather than under
`stm32f4/`.

## I2C — `driver.stm32f4.i2c`

```cpp
import driver.stm32f4.i2c;
using driver::stm32f4::I2c;

I2c g_i2c1{
    *I2C1,
    {
        .clockSpeed = 400000,
        .fastMode = true,
    },
};
```

Methods (from `II2c`):

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
import driver.stm32f4.uart;
using driver::stm32f4::Uart;
using driver::stm32f4::UartMode;
using driver::Parity;

// Interrupt mode (default)
Uart<512, 256> g_uart2{
    *USART2, USART2_IRQn,
    {
        .baudrate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = Parity::None,
    },
};

extern "C" void USART2_IRQHandler() { g_uart2.irqHandler(); }
```

Template parameters: `Uart<RxBufSize, TxBufSize, Mode>` where:

- `RxBufSize`, `TxBufSize` — power-of-2, minimum 16 (enforced by `static_assert`).
- `Mode` — `UartMode::Interrupt` (default) or `UartMode::Dma` (TX via DMA).

DMA mode requires extra ctor arguments and DMA ISR registration:

```cpp
Uart<512, 256, UartMode::Dma> g_uart2{
    *USART2, USART2_IRQn,
    driver::stm32f4::dmaMap::usart2_tx,
    { .baudrate = 115200, ... },
};

extern "C" void DMA1_Stream6_IRQHandler() { g_uart2.dmaTxIrqHandler(); }
```

DMA TX issues one transfer-complete IRQ per `write()` call (vs one IRQ per byte
in interrupt mode). For a 128-byte string at 115200 baud, CPU time spent in ISR
drops from ~11 ms to under 50 µs.

## SPI — `driver.stm32f4.spi`

```cpp
import driver.stm32f4.spi;
using driver::stm32f4::Spi;
using driver::SpiMode;
using driver::SpiBitOrder;

Spi g_spi2{
    *SPI2,
    {
        .mode = SpiMode::Mode0,
        .speed = 10'000'000,
        .bitOrder = SpiBitOrder::MsbFirst,
    },
};
```

`Spi` selects `PCLK1` for SPI2/SPI3 (APB1) and `PCLK2` for SPI1/SPI4/SPI5/SPI6
(APB2) when computing the BR divisor.

DMA support for SPI is not yet implemented; the stream/channel mapping is
reserved in `driver.stm32f4.dma::dmaMap` for a future PR.

## DMA — `driver.stm32f4.dma`

```cpp
import driver.stm32f4.dma;
using driver::stm32f4::DmaStream;
using driver::stm32f4::DmaConfig;
using driver::stm32f4::DmaDir;
using driver::stm32f4::DmaPrio;

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
