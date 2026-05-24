# Драйверы

Все драйверы лежат в `sdk/drivers/include/driver/`. Для каждой периферии
есть абстрактный интерфейс в `interface/i_*.cppm` и конкретная реализация для
STM32F4 в `stm32f4/*.cppm`.

## Правила

- Конфиги **без дефолтов** — каждое поле задаётся явно.
- Доступ к MMIO — только через `driver::reg::set/clear/write/read/get/modify`.
  Сырые `|=`, `&=`, присваивания к `volatile uint32_t` регистрам (`RCC->...`,
  `CR1` периферии, DMA stream `NDTR` и т. п.) — запрещены.
- Глобалы предпочтительнее static-local и smart-pointers; периферия живёт
  всё время работы программы.
- Никаких сырых указателей на периферию — только ссылки (`GPIO_TypeDef&`).

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

Свободная функция `gpio({...})` — `consteval`-валидатор: бросает (на этапе
компиляции) при `pin > 15`, `mode == None`, отсутствующих `speed`/`type` для
Output / AlternateFunction, `af > 15` для AF.

Тактирование GPIO-порта включается в конструкторе `GpioPin` (смотрит на адрес
порта и устанавливает соответствующий бит `RCC_AHB1ENR_GPIOxEN`).

### `NullGpioPin` — `driver.null_gpio` (v0.1.4)

Пустая реализация `IGpioPin`. Используйте, когда сенсор или драйвер
принимает `IGpioPin&` под CS-линию, **впаянную** в плату (например, SPI
flash с CS, подтянутым к GND джампером).

```cpp
import driver.null_gpio;
import driver.stm32f4.spi;

driver::NullGpioPin null_cs;
W25q32 flash{spi, null_cs, {.id = 0xEF4016}};  // CS hardwired — без переключений
```

Все четыре метода `IGpioPin` (`set`, `reset`, `toggle`, `read`) — это
inline-пустышки, поэтому компилятор обычно девиртуализует их в ничто,
если конкретный тип известен в точке вызова. Никакого доступа к
регистрам, тактирования и состояния периферии. Модуль сознательно
chip-agnostic и лежит на верхнем уровне в
`sdk/drivers/include/driver/null_gpio.cppm`, а не под `stm32f4/`.

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

Методы (из `II2c`):

- `Status write(uint8_t addr, std::span<const uint8_t> data)`
- `Status read(uint8_t addr, std::span<uint8_t> data)`
- `Status writeReg(uint8_t addr, uint8_t reg, std::span<const uint8_t> data)`
- `Status readReg(uint8_t addr, uint8_t reg, std::span<uint8_t> data)`
- `Status probe(uint8_t addr)`

Multi-byte read реализован по closing-sequence из RM0090 §27.3.3 (ветки
N=1 / N=2 / N≥3). Драйвер входит в критические секции (`taskENTER_CRITICAL` под
FreeRTOS, `__disable_irq` иначе) вокруг окон BTF → STOP → DR-read, чтобы ISR
не нарушила закрытие транзакции. `sendAddress()` гонит `ADDR` против `AF` —
на NACK транзакция прерывается почти сразу; сканирование шины 0x03..0x77
занимает ≈ 200 мс.

Тактирование — ответственность вызывающего, обычно в `__initialize_hardware()`:

```cpp
driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);
```

## UART — `driver.stm32f4.uart`

```cpp
import driver.stm32f4.uart;
using driver::stm32f4::Uart;
using driver::stm32f4::UartMode;
using driver::Parity;

// Interrupt mode (по умолчанию)
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

Template-параметры: `Uart<RxBufSize, TxBufSize, Mode>`, где:

- `RxBufSize`, `TxBufSize` — степень двойки, минимум 16 (`static_assert`).
- `Mode` — `UartMode::Interrupt` (по умолчанию) или `UartMode::Dma` (TX через DMA).

DMA-режим требует дополнительных параметров конструктора и регистрации DMA ISR:

```cpp
Uart<512, 256, UartMode::Dma> g_uart2{
    *USART2, USART2_IRQn,
    driver::stm32f4::dmaMap::usart2_tx,
    { .baudrate = 115200, ... },
};

extern "C" void DMA1_Stream6_IRQHandler() { g_uart2.dmaTxIrqHandler(); }
```

DMA TX выдаёт одно transfer-complete IRQ на `write()` (vs одно IRQ на байт
в interrupt mode). Для 128-байтной строки на 115200 baud время CPU в ISR
падает с ~11 мс до менее 50 мкс.

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

`Spi` выбирает `PCLK1` для SPI2/SPI3 (APB1) и `PCLK2` для SPI1/SPI4/SPI5/SPI6
(APB2) при расчёте BR-делителя.

DMA для SPI пока не реализован; mapping stream/channel зарезервирован в
`driver.stm32f4.dma::dmaMap` под будущий PR.

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

`DmaStream` — RAII-обёртка. Mapping периферия ↔ stream/channel для F407VG
живёт в namespace `dmaMap`:

| Периферия | Stream | Channel |
|-----------|--------|---------|
| USART1 TX | DMA2/Stream 7 | 4 |
| USART1 RX | DMA2/Stream 5 | 4 |
| USART2 TX | DMA1/Stream 6 | 4 |
| USART2 RX | DMA1/Stream 5 | 4 |
| SPI1 TX | DMA2/Stream 3 | 3 |
| SPI1 RX | DMA2/Stream 0 | 3 |
| SPI2 TX | DMA1/Stream 4 | 0 |
| SPI2 RX | DMA1/Stream 3 | 0 |

## Внутренний flash — `driver.stm32f4.internal_flash`

```cpp
import driver.stm32f4.internal_flash;
using driver::stm32f4::InternalFlash;

InternalFlash g_flash;
g_flash.eraseSector(11);
g_flash.write(0x080E0000, std::span{data});
```

Расположение секторов зависит от чипа: у F407VG 12 секторов (16K×4, 64K×1,
128K×7). `InternalFlash` включает тактирование контроллера и обрабатывает
последовательности unlock / program / lock.
