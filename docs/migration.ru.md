# Заметки по апгрейду

Эта страница перечисляет, что нужно изменить в коде вашего проекта при
обновлении версии SDK. Проект ещё в pre-1.0 — публичные API могут уточняться
между релизами. Фиксируйте конкретный тег в `stmproject.toml` (вместо
`develop`) и обновляйтесь осознанно.

## Рекомендованный workflow

1. На отдельной ветке вашего проекта обновите `[sdk] version` до нового тега.
2. `stmtool sdk update --version <тег>`.
3. Перепрогон форматтера: `clang-format -i src/**/*.cpp src/**/*.cppm`.
4. `stmtool build --native --clean`. Разбирайте ошибки компиляции по одной.
5. Прошейте на железо и убедитесь, что ваш smoke-test проходит.
6. Сливайте, когда всё зелёное.

## Апгрейд на v0.1.7

### Интерфейсы драйверов и сенсоров теперь C++20-концепты

Виртуальные базовые классы `IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash`
(драйверы) и `IImu`, `IDisplay`, `IExternalFlash` (сенсоры) теперь —
**C++20-концепты**, а не классы. Драйверы и сенсоры больше от них не
наследуются; вызовы шины внутри сенсоров прямые (без vtable).

Если ваш проект использует только конкретные типы (обычный случай — так
делают все встроенные шаблоны), единственные возможные правки — переименование
констант в `W25q32Spec` и приведения сырых указателей `sensor::Xxx*` ниже.

- **Вы брали интерфейс по ссылке** (`II2c&`, `ISpi&`, `IGpioPin&`, интерфейс
  сенсора, …). Концепт — не тип, поэтому `driver::II2c& bus` больше не
  компилируется. Берите конкретный тип или делайте функцию/класс ограниченным
  шаблоном:

  ```cpp
  // было
  void useBus(driver::II2c &bus);
  // стало — ограниченный параметр шаблона (стиль, принятый в SDK)
  template <driver::II2c I2cDriver>
  void useBus(I2cDriver &bus);
  ```

- **Сенсоры теперь шаблоны класса по типу шины.** Создание не меняется
  благодаря CTAD: `sensor::Mpu6050 g{i2c, {...}}` по-прежнему работает и
  выводит `Mpu6050<I2c>`. Но «голый» тип назвать уже нельзя — например,
  `sensor::Mpu6050 *` ill-formed. Используйте `decltype(&g)`:

  ```cpp
  // было
  auto *mpu = static_cast<sensor::Mpu6050 *>(ctx);
  // стало
  auto *mpu = static_cast<decltype(&g_mpu)>(ctx);
  ```

- **Константы устройства `W25q32` переехали в `sensor::W25q32Spec`.** Поскольку
  `W25q32` теперь шаблон, `sensor::W25q32::SECTOR_SIZE` требовал бы шаблонных
  аргументов. Константы лежат в нешаблонном `sensor::W25q32Spec`:

  ```cpp
  // было
  sensor::W25q32::JEDEC_W25Q32JV
  // стало
  sensor::W25q32Spec::JEDEC_W25Q32JV       // также CAPACITY, SECTOR_SIZE, PAGE_SIZE
  ```

- **`NullGpioPin` теперь обычный `struct`** (больше не наследует виртуальную
  базу). Использование не меняется.

### Все методы драйверов/сенсоров, возвращающие значение, теперь `[[nodiscard]]`

Раньше `[[nodiscard]]` нёс только тип `Status`. Теперь помечены и сами методы,
включая возвращающие счётчик или геометрию (`Uart::write`/`read` → `size_t`,
`Flash::sectorSize`/`sectorCount`, `Display::width`/`height`,
`IExternalFlash::capacity`/`jedecId`, …). Под `-Werror` проигнорированный
результат — ошибка сборки. Типичный случай — fire-and-forget запись в UART;
сбрасывайте результат явно:

```cpp
// было
g_uart2.write({data, len});
// стало
(void) g_uart2.write({data, len});
```

### `driver::Status` теперь `[[nodiscard]]`

`Status` (и новый `Result<T>`) помечены атрибутом `[[nodiscard]]`. Под
обязательным для SDK `-Werror` любой call-сайт, который **игнорировал**
возврат `Status`, больше не компилируется. Это может всплыть в downstream-коде,
вызывавшем `write` / `read` / `eraseSector` / `init` и сбрасывавшем результат.

Поправьте каждое место одним из двух способов:

```cpp
// 1. Обработать ошибку (предпочтительно):
if (g_flash.eraseSector(0) != driver::Status::Ok) {
    // лог / повтор / halt
}

// 2. Явно сбросить, когда ошибка действительно неважна
//    (например, потеря байта в полном ISR-кольце):
(void) _rxBuf.push(byte);
```

`Result<T>`, `DRV_TRY` и `DRV_TRY_ASSIGN` — новые аддитивные API, они не ломают
существующий код. См. [драйверы](modules/drivers.ru.md).

## Апгрейд с v0.1.2

### `GpioConfig` теперь aggregate, валидация через `gpio({...})`

Позиционный `consteval GpioConfig(...)` конструктор удалён, заменён на
обычный aggregate + свободный `consteval gpio()`-валидатор. Новая форма
обязательна во всех call-сайтах.

**Было (v0.1.2):**

```cpp
GpioPin led{*GPIOD, GpioConfig{12, PinMode::Output, PullMode::None,
                               OutputSpeed::Low, OutputType::PushPull}};
```

**Стало (latest):**

```cpp
GpioPin led{*GPIOD, gpio({.pin = 12, .mode = PinMode::Output,
                          .pull = PullMode::None, .speed = OutputSpeed::Low,
                          .type = OutputType::PushPull})};
```

Trailing comma после последнего designated-инициализатора удерживает
clang-format от схлопывания конфига обратно в одну строку.

### GPIO теперь per-pin

Интерфейс переработан с per-port (`IGpio`) на per-pin (`IGpioPin`). Каждый
`GpioPin` владеет одним сконфигурированным пином; то, что раньше было
методами уровня порта, теперь у отдельных пинов.

**Было:**

```cpp
Gpio g_porta{*GPIOA};
g_porta.write(5, true);
```

**Стало:**

```cpp
GpioPin g_pa5{*GPIOA, gpio({.pin = 5, .mode = PinMode::Output, ...})};
g_pa5.write(true);
```

### `Uart<>` получил третий template-параметр (backward compatible)

`Uart<RxBufSize, TxBufSize, Mode>`, где `Mode` по умолчанию
`UartMode::Interrupt`. Существующий код `Uart<256, 256>` продолжает работать
без правок. DMA TX — opt-in:

```cpp
Uart<512, 256, UartMode::Dma> g_uart2{ ... };
```

## Новые доступные модули

После апгрейда становятся доступны (без дополнительной настройки):

- `driver.stm32f4.dma` — `DmaStream` RAII-обёртка + таблица `dmaMap`.
- `sensor.display` — интерфейс `IDisplay`.
- `sensor.ssd1306` — драйвер SSD1306 OLED.
- `sensor.external_flash` — интерфейс `IExternalFlash`.
- `sensor.w25q32` — драйвер SPI flash W25Q32.

## Изменения в clang-format

В `.clang-format` добавлены:

- `BinPackArguments: false`, `BinPackParameters: false`
- `AllowAllArgumentsOnNextLine: false`, `AllowAllParametersOfDeclarationOnNextLine: false`
- `AlignAfterOpenBracket: BlockIndent`
- `Cpp11BracedListStyle: false`

При `clang-format -i` после апгрейда — ожидайте шумные diff'ы на call-сайтах
с aggregate-конфигами. Используйте trailing comma после последнего
designated-инициализатора, чтобы детерминированно сохранить multi-line.

## Поведение I2C

Если вы читаете multi-byte буферы по I2C на 400 кГц — путь чтения теперь
строго по спецификации RM0090 §27.3.3. Раньше драйвер мог читать мусор в
последних 1-2 байтах `read()` / `readReg()` на высокой тактовой; сенсоры с
короткими блоками (например, 14-байтный блок MPU6050) страдали сильнее.
Правок в коде не требуется — фикс прозрачен.

`I2c::probe()` теперь гонит `ADDR` против `AF` для быстрого детектирования
NACK. Сканирование шины, занимавшее ~58 с, теперь укладывается в ~200 мс.

## Защита проекта на будущее

- Фиксируйте `[sdk] version` на тег в `main`/`master`; `develop` используйте
  только на экспериментальных ветках.
- Если проект сильно зависит от SDK — настройте CI на сборку против
  нескольких тегов одновременно.
- Подпишитесь на GitHub Releases, чтобы видеть новые теги.
