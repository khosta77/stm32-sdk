# Драйверы

Все драйверы лежат в `sdk/drivers/include/driver/`. Для каждой периферии
есть C++20-**концепт** в `interface/i_*.cppm` (compile-time контракт) и
конкретная реализация для STM32F4 в `stm32f4/*.cppm`, которая ему
удовлетворяет.

С v0.1.7 интерфейсы — это **концепты, а не виртуальные базовые классы**
(`IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash`). Драйвер просто предоставляет
нужные методы — без наследования, без vtable на вызовах шины. Код, которому
нужна обобщённая периферия, принимает её как ограниченный параметр шаблона
(`template <driver::II2c I2cDriver>`), а тестовый дубль — это обычный `struct`,
удовлетворяющий концепту. Каждая реализация заканчивается self-check'ом
`static_assert(IXxx<Impl>)`, поэтому дрейф сигнатуры — ошибка компиляции.

## Правила

- Конфиги **без дефолтов** — каждое поле задаётся явно.
- Доступ к MMIO — только через `driver::reg::set/clear/write/read/get/modify`.
  Сырые `|=`, `&=`, присваивания к `volatile uint32_t` регистрам (`RCC->...`,
  `CR1` периферии, DMA stream `NDTR` и т. п.) — запрещены.
- Глобалы предпочтительнее static-local и smart-pointers; периферия живёт
  всё время работы программы.
- Никаких сырых указателей на периферию — только ссылки (`GPIO_TypeDef&`).

## Обработка ошибок — `driver.types` (v0.1.5)

Драйверы сообщают об ошибках через `enum class Status : uint8_t` (`Ok`,
`None`, `Timeout`, `Nack`, `BusError`, `Busy`, `InvalidArg`,
`HardwareError`). С v0.1.5 `Status` помечен `[[nodiscard]]`: каждый
возвращаемый `Status` нужно потребить либо явно сбросить через `(void)`. Под
`-Werror` проигнорированный `Status` — ошибка сборки. С v0.1.7 `[[nodiscard]]`
помечен **каждый возвращающий значение метод драйвера и сенсора** — включая
счётчики байт (`Uart::write`/`read`) и геометрию (`Flash::sectorSize`,
`Display::width`, …), поэтому fire-and-forget запись в UART требует явного
`(void)`.

```cpp
import driver.types;

if (g_i2c.write(addr, payload) != driver::Status::Ok) { /* обработать */
}

(void) _rxBuf.push(byte);  // намеренный сброс внутри ISR
```

### `Result<T>`

`Result<T>` несёт **либо** значение `T`, **либо** ошибку `Status`
(1-байтовый тег, без аллокаций, без исключений). Помечен `[[nodiscard]]` и
полностью `constexpr`. `T` должен быть тривиально разрушаемым (что верно для
POD-payload, возвращаемых драйверами).

```cpp
import driver.types;
using driver::Result;
using driver::Status;

Result<uint16_t> r = readAdc();  // значение или ошибка
if (r.ok()) {
  uint16_t v = r.value();  // предусловие: ok() — иначе UB
}
uint16_t safe = r.valueOr(0);  // никогда не UB
Status st = r.status();        // Ok при успехе, иначе — ошибка
```

`value()` при ошибке — неопределённое поведение (как `*std::optional`);
используйте `valueOr()` или проверяйте `ok()`. Переданный как ошибка
`Status::Ok` нормализуется в `None`, поэтому ошибочный `Result` никогда не
сообщает `ok()`.

### `DRV_TRY` / `DRV_TRY_ASSIGN` — ранний возврат

Макросы не экспортируются модулем C++20, поэтому хелперы в стиле Rust-`?`
лежат в текстовом заголовке `driver/try.hpp`. Подключайте его **после**
`import driver.types;`; окружающая функция должна возвращать `Status` (или
`Result<U>`).

```cpp
import driver.types;
#include "driver/try.hpp"

driver::Status configure() {
  DRV_TRY(g_i2c.writeReg(addr, REG_CTRL, ctrl));  // вернуть Status, если не Ok

  uint16_t raw = 0;
  DRV_TRY_ASSIGN(raw, readAdc());  // распаковать Result или вернуть ошибку
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

Свободная функция `gpio({...})` — `consteval`-валидатор: бросает (на этапе
компиляции) при `pin > 15`, `mode == None`, отсутствующих `speed`/`type` для
Output / AlternateFunction, `af > 15` для AF.

Тактирование GPIO-порта включается в конструкторе `GpioPin` (смотрит на адрес
порта и устанавливает соответствующий бит `RCC_AHB1ENR_GPIOxEN`).

### `NullGpioPin` — `driver.null_gpio` (v0.1.4)

Пустой `struct`, удовлетворяющий концепту `IGpioPin`. Используйте, когда
сенсор принимает CS-пин, **впаянный** в плату (например, SPI flash с CS,
подтянутым к GND джампером).

```cpp
import driver.null_gpio;
import driver.stm32f4.spi;

driver::NullGpioPin null_cs;
// W25q32 — шаблон по типам шины и CS; CTAD выводит их из аргументов:
sensor::W25q32 flash{
    spi,
    null_cs,
    {.expectedJedecId = sensor::W25q32Spec::JEDEC_W25Q32JV,
     .busyPollLoops = 5000000U}
};  // CS hardwired — без переключений
```

Четыре метода пина (`set`, `reset`, `toggle`, `read`) — inline-пустышки.
С v0.1.7 `NullGpioPin` — обычный `struct` (а не наследник виртуальной базы),
поэтому vtable нет вовсе — вызовы компилируются в ничто. Никакого доступа к
регистрам, тактирования и состояния периферии. Модуль сознательно
chip-agnostic и лежит на верхнем уровне в
`sdk/drivers/include/driver/null_gpio.cppm`, а не под `stm32f4/`.

## I2C — `driver.stm32f4.i2c`

```cpp
import driver.i2c; // I2cConfig + валидатор i2c()
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

`i2c({...})` — `consteval`-валидатор (как `gpio()` / `exti()`): бросает на этапе
компиляции при `clockSpeed` вне `[1, 400000]` или `clockSpeed > 100000` без
`fastMode`. `I2cConfig` и `i2c()` живут в interface-модуле `driver.i2c` —
импортируйте его рядом с `driver.stm32f4.i2c` (реализация его не реэкспортирует).

Методы (требуемые концептом `II2c`):

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
import driver.uart; // UartConfig, DataBits/StopBits/Parity, uart()
import driver.stm32f4.uart;
using driver::DataBits;
using driver::Parity;
using driver::StopBits;
using driver::uart;
using driver::stm32f4::Uart;
using driver::stm32f4::UartMode;

// Interrupt mode (по умолчанию)
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

`uart({...})` — `consteval`-валидатор: бросает на этапе компиляции при
`baudrate == 0` или незаданных `dataBits`/`stopBits` (`DataBits::None` /
`StopBits::None`). `UartConfig` и enum'ы живут в `driver.uart`.

Template-параметры: `Uart<RxBufSize, TxBufSize, Mode>`, где:

- `RxBufSize`, `TxBufSize` — степень двойки, минимум 16 (`static_assert`).
- `Mode` — `UartMode::Interrupt` (по умолчанию) или `UartMode::Dma` (TX через DMA).

DMA-режим требует дополнительных параметров конструктора и регистрации DMA ISR:

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

DMA TX выдаёт одно transfer-complete IRQ на `write()` (vs одно IRQ на байт
в interrupt mode). Для 128-байтной строки на 115200 baud время CPU в ISR
падает с ~11 мс до менее 50 мкс.

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

`spi({...})` — `consteval`-валидатор: бросает на этапе компиляции при
`clockHz == 0`, незаданных `mode`/`dataSize` (`SpiMode::None` /
`SpiDataSize::None`) или `SpiDataSize::Bits16`. `SpiConfig`, `SpiMode`
(`Mode0..Mode3`, CPOL/CPHA) и `SpiDataSize` живут в `driver.spi`.

> Поддерживается только `SpiDataSize::Bits8`. Драйвер гонит регистр данных по
> одному байту, поэтому 16-битный размер фрейма испортил бы фрейминг; с v0.2.0
> `spi({...})` отклоняет `Bits16` на этапе компиляции. Значение enum
> зарезервировано под будущий 16-битный путь.

`Spi` выбирает `PCLK1` для SPI2/SPI3 (APB1) и `PCLK2` для SPI1/SPI4/SPI5/SPI6
(APB2) при расчёте BR-делителя.

DMA для SPI пока не реализован; mapping stream/channel зарезервирован в
`driver.stm32f4.dma::dmaMap` под будущий PR.

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

## EXTI — `driver.stm32f4.exti` (v0.1.10)

Драйвер линий внешних прерываний: маршрутизирует вывод GPIO на линию EXTI,
настраивает фронт и вызывает captureless-колбэк из ISR. Концепт `driver::IExti`,
агрегат `ExtiConfig` и валидатор `exti({...})` живут в `driver.exti`; реализация
для F4 `ExtiLine` — в `driver.stm32f4.exti`.

```cpp
import driver.exti;
import driver.stm32f4.exti;
using driver::exti;
using driver::ExtiPort;
using driver::ExtiTrigger;
using driver::stm32f4::ExtiLine;

// Привязка линии к методу; колбэк выполняется в контексте прерывания.
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

- `exti({...})` — `consteval`-валидатор: бросает (на этапе компиляции) при
  `line > 15` или `priority > 15` (у F4 4 бита приоритета NVIC).
- Конструктор включает такт SYSCFG, выбирает порт в `SYSCFG_EXTICR`, ставит
  `RTSR`/`FTSR` под фронт, гасит pending-бит, задаёт приоритет NVIC, включает IRQ
  и снимает маску `IMR`. Значения `ExtiPort` ложатся прямо в 4-битное поле
  `EXTICR` (A = 0 … H = 7).
- `irqHandler()` проверяет бит `PR` своей линии, гасит его (write-1-to-clear) и
  вызывает привязанный колбэк. У линий 0–4 отдельный IRQ; 5–9 делят
  `EXTI9_5_IRQn`, 10–15 — `EXTI15_10_IRQn`. На общем векторе вызывайте
  `irqHandler()` **каждого** объекта линии — каждый проверяет свой pending-бит.
- `enable()` / `disable()` переключают маску; `pending()` читает `PR`;
  `clearPending()` гасит его вручную.

Сам вывод GPIO настраивается отдельно через `GpioPin` (режим входа). Чтобы
вынести работу из ISR, колбэк вызывает `executor.postFromISR(workItem)` —
приоритет ISR (здесь 6) должен быть численно ≥
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 у F4), иначе вызов недопустим.
См. шаблон `button-events-demo`.

## CRC-32 — `driver.crc` / `driver.soft_crc` / `driver.stm32f4.crc` (v0.2.3)

CRC-32/IEEE — тот вариант, что используют zlib, gzip, PNG и Ethernet, поэтому
контрольную сумму, посчитанную на плате, можно проверить на рабочей станции
через `python3 -c 'import zlib; print(hex(zlib.crc32(data)))'` независимо от
того, какая микросхема флэш-памяти выдала эти байты. Концепт `driver::ICrc` и
константы `Crc32Spec` живут в `driver.crc`, переносимая реализация `SoftCrc` —
в `driver.soft_crc`, драйвер периферии F4 `Crc` — в `driver.stm32f4.crc`.

```cpp
import driver.crc;
import driver.soft_crc;
import driver.stm32f4.crc;
using driver::Crc32Spec;
using driver::SoftCrc;

// Аппаратный блок сам включает свой такт AHB1 в конструкторе.
driver::stm32f4::Crc g_crc{*CRC};

// Однократный вызов.
const uint32_t sum = g_crc.compute({data, len});

// Потоковый режим: считаем сумму партиции флэша страница за страницей.
g_crc.reset();
for (uint32_t off = 0; off < size; off += sizeof(page)) {
  (void) flash.read(base + off, page);
  g_crc.update({page, sizeof(page)});
}
const uint32_t total = g_crc.value();

// Тот же API без периферии — годится для host-тестов и constexpr.
SoftCrc soft;
const uint32_t reference = soft.compute({data, len});
```

- Оба типа моделируют `driver::ICrc`: потоковая тройка `reset()` /
  `update(span)` / `value()` плюс однократный `compute(span)`. `value()` —
  чистое чтение: повторный вызов, в том числе посреди потока, вернёт то же
  число и не израсходует состояние.
- Параметры фиксированы: полином `0x04C11DB7` (reflected `0xEDB88320`),
  начальное значение `0xFFFFFFFF`, реверс входа и выхода, финальный XOR
  `0xFFFFFFFF`. Контрольное значение `Crc32Spec::CHECK` равно `0xCBF43926`
  (CRC-32 строки `"123456789"`) и проверяется на этапе компиляции
  `consteval`-самопроверкой в `driver.soft_crc`.
- Принимаются буферы **любой** длины. У F4 `CRC_DR` берёт только 32-битные
  слова (побайтовая подача появилась с F7), поэтому `Crc` отдаёт железу целые
  слова, а хвостовые 1–3 байта домешивает программно при чтении `value()`.
  Чанки любого размера можно подавать в любом порядке.
- Блок F4 считает MSB-first без реверсов и без финального XOR, то есть сам по
  себе он не даёт CRC-32/IEEE. `Crc` соединяет два домена через `__RBIT` на
  каждом входном слове и на результате: после `RESET` в `CRC_DR` читается
  `0xFFFFFFFF`, чей реверс в точности равен начальному значению.
- В `CRC_CR` есть единственный бит `RESET` — ни полином, ни реверсы у F4 не
  настраиваются (это появилось у F7/L4/G0/H7). Поэтому нет ни структуры
  конфигурации, ни `consteval`-валидатора.
- **Один экземпляр на чип.** Периферия хранит глобальное состояние, и два
  чередующихся потребителя портят результат друг другу. Держите единственный
  `Crc` и сериализуйте доступ снаружи (`rtos::Mutex`) либо используйте
  `SoftCrc` — он реентерабелен.
- `SoftCrc` не зависит от CMSIS, весь `constexpr` и битовый (8 шагов на байт,
  ноль байт в `.rodata`), поэтому работает и на хосте, и внутри `consteval`, и
  на чипах без блока CRC. Он покрыт `tests/host/test_crc.cpp`; аппаратный путь
  сверяется с ним на плате шаблоном `unit-test-demo`.

## CircularBuffer — `driver.circular_buffer`

Кольцевой буфер фиксированной ёмкости с одним производителем и одним
потребителем (SPSC), используемый внутри `Uart<>` для RX/TX FIFO. Он CMSIS-free
и lock-free (два индекса `std::atomic<size_t>` с acquire/release-упорядочиванием),
поэтому безопасен через границу ISR/задача и покрыт host-тестами.

```cpp
import driver.circular_buffer;
using driver::CircularBuffer;

CircularBuffer<uint8_t, 256> fifo;  // N должно быть степенью двойки
```

- `N` должно быть степенью двойки (проверяется `static_assert`). Один слот
  зарезервирован для различения полного и пустого, поэтому полезная ёмкость —
  `N - 1`.
- `push(item)` / `pop(item)` переносят один элемент и возвращают `Status::Ok`
  либо `Status::Busy`, когда буфер полон / пуст соответственно.
- `write(src, len)` / `read(dst, maxLen)` переносят блок и возвращают реально
  перенесённое число (ограничено свободным местом / доступными данными).
- `size()`, `free_space()`, `empty()`, `full()`, `capacity()` запрашивают
  состояние. Все — `[[nodiscard]]`.
