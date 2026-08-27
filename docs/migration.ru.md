# Заметки по апгрейду

Эта страница перечисляет, что нужно изменить в коде вашего проекта при
обновлении версии SDK. Проект ещё в pre-1.0 — публичные API могут уточняться
между релизами. Фиксируйте конкретный тег в `stmproject.toml` (вместо
`develop`) и обновляйтесь осознанно.

## Рекомендованный workflow

1. На отдельной ветке вашего проекта обновите `[sdk] version` до нового тега.
2. `stmtool sdk update --version <тег>`.
3. Перепрогон форматтера: `clang-format -i src/**/*.cpp src/**/*.cppm`.
4. `stmtool build --clean`. Разбирайте ошибки компиляции по одной.
5. Прошейте на железо и убедитесь, что ваш smoke-test проходит.
6. Сливайте, когда всё зелёное.

## Новое в v0.2.4

v0.2.4 — **ломающий** релиз. Новой функциональности прошивки в нём нет: он
убирает `throw` из SDK, выключает исключения и RTTI и расшивает привязку к F4
в семейно-нейтральном коде. Правок в исходниках требуется две.

### 1. Валидаторы конфигов принимают конфиг параметром шаблона

`gpio()`, `exti()`, `i2c()`, `spi()` и `uart()` сообщали о неверном конфиге
через `throw` — именно это блокировало `-fno-exceptions`: GCC отвергает сам
токен даже внутри `consteval`-функции, а диалект исключений зашит в BMI каждого
модуля, поэтому флаг нельзя было применить к части дерева.

Теперь конфиг — параметр шаблона, а каждое правило — `static_assert`.
Перенесите фигурные скобки внутрь угловых и добавьте `()`:

```cpp
// было
GpioPin g_led{*GPIOD, gpio({.pin = 12, .mode = PinMode::Output,
                            .speed = OutputSpeed::Low,
                            .type = OutputType::PushPull})};

// стало
GpioPin g_led{*GPIOD, gpio<{.pin = 12, .mode = PinMode::Output,
                            .speed = OutputSpeed::Low,
                            .type = OutputType::PushPull}>()};
```

Правка механическая: `name({...})` превращается в `name<{...}>()`. Имена полей,
значения и правила проверки не изменились.

Диагностика при этом улучшилась. `throw` давал
`error: expression '<throw-expression>' is not a constant expression`, и текст
сообщения был виден только потому, что GCC выводит эхом исходную строку.
`static_assert` несёт текст как саму диагностику, GCC добавляет note с
подставленными значениями, и сообщаются **все** нарушенные правила за один
проход, а не только первое:

```
error: static assertion failed: GpioConfig: pin must be in [0, 15]
note: the comparison reduces to '(42 <= 15)'
error: static assertion failed: GpioConfig: OutputSpeed required for Output/AlternateFunction
```

### 2. Карта партиций стала типом

По той же причине `storage::partitionTable()` и `PartitionTable<N>` заменены на
`storage::PartitionMap`. Имя партиции теперь хранится встроенно (не более 15
символов), а не как `const char *`: указатель на строковый литерал не является
допустимым аргументом шаблона.

```cpp
// было
constexpr auto kPartitions = storage::partitionTable<sensor::W25q32Spec>({
    {.name = "boot",   .offset = 0x000000, .size = 0x010000},
    {.name = "config", .offset = 0x010000, .size = 0x001000},
});
storage::Partition config{device, kPartitions.find("config")};
for (size_t i = 0; i < kPartitions.count(); ++i) { use(kPartitions[i]); }

// стало
using Partitions = storage::PartitionMap<
    sensor::W25q32Spec,
    {.name = "boot",   .offset = 0x000000, .size = 0x010000},
    {.name = "config", .offset = 0x010000, .size = 0x001000}>;
storage::Partition config{device, Partitions::find<"config">()};
for (size_t i = 0; i < Partitions::count(); ++i) { use(Partitions::at(i)); }
```

`operator[]` стал `at(i)`, а `PartitionSpec::name` теперь `PartitionName`, а не
`const char *` — там, где вы его печатали, используйте `.name.c_str()`.
`Partition::name()` не изменился и по-прежнему возвращает `const char *`.

### 3. Исключения и RTTI выключены везде

`-fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables`
теперь применяются к `stm32_core` и ко всем библиотекам SDK, а значит и к вашей
прошивке. `try` / `catch` / `throw` / `dynamic_cast` / `typeid` в прикладном
коде перестают компилироваться. В рантайме SDK ничего не бросал, поэтому
выброшен только мёртвый вес:

| шаблон | flash до | после |
|---|---|---|
| `bare-metal/blink` | 5892 | 1724 (−71%) |
| `freertos/signal-bus-demo` | 21796 | 14236 (−35%) |
| `freertos/w25q32-flash-test` | 23796 | 16344 (−31%) |
| `freertos/imu-flash-oled-demo` | 25812 | 18016 (−30%) |

Флаги нельзя применить выборочно: GCC хранит диалект в BMI каждого модуля, и
рассогласование даёт
`error: module ...: language dialect differs 'C++20', expected 'C++20/no-exceptions'`.

### 4. Новое, действий не требует

- `out/generated/version.hpp` генерируется из `git describe` вашего проекта и
  лежит на include-пути любого проекта. `firmware::VERSION`,
  `VERSION_MAJOR/MINOR/PATCH`, `VERSION_TAGGED`, `VERSION_DIRTY`. Проект без
  тега сообщает `v0.0.0-untagged` — это запасной вариант, а не ошибка.
- `sdk/chips.json` — индекс чипов; шестнадцать заглушек «not yet supported» в
  `sdk/cmake/families/` удалены. Неподдерживаемый чип теперь сообщает список
  поддерживаемых семейств из этого одного файла.
- `STM32_LOG_BACKEND_SEL_ITM` зависит от `STM32_CORE_HAS_ITM`, поэтому на
  Cortex-M0/M0+ он отвергается на этапе configure, а не падает внутри модуля.
  На F4 ничего не меняется.
- В CI появился QEMU-гейт рантайма — см. [Эмуляцию](emulation.ru.md).

## Новое в v0.2.3

v0.2.3 добавляет CRC-32 и слой флэш-партиций. Релиз **аддитивный** — ни один
существующий символ не изменился, для обновления править проект не нужно.

### Флэш-партиции (`STM32_USE_STORAGE`)

Гейт, приехавший пустым в v0.2.2, наполнен. Включите его в `.config` (он
зависит от `STM32_USE_DRIVERS`) и добавьте `stm32_storage` в
`target_link_libraries` своего `CMakeLists.txt`:

```cpp
import storage.flash_device;
import storage.partition;

constexpr auto kPartitions = storage::partitionTable<sensor::W25q32Spec>({
    {.name = "boot",    .offset = 0x000000, .size = 0x010000},
    {.name = "config",  .offset = 0x010000, .size = 0x001000},
    {.name = "storage", .offset = 0x011000, .size = 0x3EF000},
});

storage::ExternalFlashDevice device{flash};
storage::Partition config{device, kPartitions.find("config")};
(void) config.write(0, {data, len});   // смещение 0 == начало "config"
```

Перекрытия, невыравненные границы, выход за пределы носителя и опечатки в
именах — ошибки компиляции. Смещения относительны партиции; обращение за её
конец возвращает `Status::InvalidArg`. `checksum()` считает контрольную сумму
партиции любым движком `driver::ICrc`. Полное руководство:
[Хранилище](modules/storage.ru.md).

Символ `STM32_USE_STORAGE` переехал из `sdk/drivers/Kconfig` в собственный
фрагмент `sdk/storage/Kconfig`. Имя и зависимость `depends on
STM32_USE_DRIVERS` не изменились, поэтому существующие `.config` продолжают
работать.

### CRC-32

Новая поверхность доступна всегда, когда включён `STM32_USE_DRIVERS` (нового
Kconfig-символа не появилось):

1. **Концепт и константы CRC-32** (`driver.crc`): `driver::ICrc` — потоковая
   тройка `reset()` / `update(span)` / `value()` плюс однократный
   `compute(span)` — и `driver::Crc32Spec` с параметрами алгоритма и
   контрольным значением `CHECK` = `0xCBF43926`.
2. **Переносимая реализация** (`driver.soft_crc`): `driver::SoftCrc` — без
   CMSIS, весь `constexpr` и реентерабельный, поэтому работает на хосте,
   внутри `consteval` и на чипах без блока CRC. Также экспортируется
   `softCrcStep(state, byte)`.
3. **Аппаратный драйвер** (`driver.stm32f4.crc`): `driver::stm32f4::Crc{*CRC}`
   — включает свой такт AHB1 в конструкторе, запись в
   `__initialize_hardware()` не нужна.

Вариант алгоритма — CRC-32/IEEE (zlib, gzip, PNG, Ethernet), поэтому сумма с
платы совпадает с `zlib.crc32` на рабочей станции. Учтите, что периферия F4 —
единственный глобальный ресурс: держите один экземпляр `Crc` и сериализуйте
доступ либо используйте `SoftCrc`, у которого общего состояния нет.

См. [Драйверы](modules/drivers.ru.md#crc-32-drivercrc-driversoft_crc-driverstm32f4crc-v023).

## Новое в v0.2.2

v0.2.2 вводит **Kconfig** как единственный источник правды о конфигурации
прошивки ([полное руководство](configuration.ru.md)). Это **жёсткий срез**:
старый путь конфигурации удалён, при обновлении SDK мигрировать должен
каждый проект.

### Действие: создать `.config` (обязательно)

Проект без `.config` больше не конфигурируется — CMake останавливается с
ошибкой. Создать его можно двумя способами:

```bash
stmtool config     # откроет menuconfig с дефолтами дерева; сохранение -> .config
```

или скопировать defconfig шаблона, с которого начинался ваш проект:

```bash
cp <sdk>/templates/freertos/blink/defconfig .config
```

Коммитьте `.config` в git — это часть проекта, как `prj.conf` в Zephyr.

### Действие: убрать старую конфигурацию из `CMakeLists.txt`

Удалите строку `set(STM32_CHIP ...)` и каждую строку `set(STM32_USE_* ...)` —
эти значения теперь приходят из `stmproject.toml` (чип) и `.config` (гейты).
Передача `-DSTM32_USE_*` или `-DSTM32_LOG_LEVEL`/`-DSTM32_LOG_BACKEND` в
командной строке больше не действует; перенесите эти решения в `.config`.

### Действие: почистить `stmproject.toml`

Мёртвые секции `[project]`, `[build]` и `[serial]` никогда не читались ни
одним инструментом — удалите их. Остаётся манифест: `[sdk] version`,
`[target] chip`, `[flash] tool`. (Версионирование проекта полноценно
вернётся с issue #90: git-тег → генерируемый `version.hpp`.)

### Изменение поведения: тюнаблы FreeRTOS теперь настраиваются

Размер heap, частота тика, число приоритетов и размеры стеков были
захардкожены в `FreeRTOSConfig.h` SDK; теперь это Kconfig-символы
(`FREERTOS_*`) с историческими значениями в качестве дефолтов. Дефолтный
`.config` собирает **бинарно идентичную** прошивку — если менять их не
нужно, делать ничего не требуется.

### Тулчейн: обновить stmtool и Docker-образ

Шагу генерации нужен `kconfiglib`, который поставляется в обновлённом
build-образе; команде `stmtool config` нужен обновлённый инструмент:

```bash
pipx upgrade stmtool
docker pull ghcr.io/khosta77/stm32-sdk-build:latest
```

## Новое в v0.2.1

v0.2.1 выносит `stmtool` из монорепо SDK в отдельный репозиторий
[`khosta77/stmtool`](https://github.com/khosta77/stmtool). **Видимых в исходниках
SDK изменений нет** — меняется только способ установки и обновления инструмента.

### Действие: переустановить `stmtool` из нового репозитория

`pip install ./tools/stmtool` больше не работает (каталог удалён). Переустановите
из отдельного репозитория:

```bash
pipx uninstall stmtool 2>/dev/null || true
pipx install git+https://github.com/khosta77/stmtool.git
```

`./install.sh` в SDK по-прежнему работает и теперь ссылается на новый
репозиторий. CLI, команды и поведение `STMSDK_PATH` / `STMTOOL_DOCKER_IMAGE` не
изменились; инструмент так же находит этот checkout SDK. `stmtool` теперь
версионируется как `v0.N` независимо от релизных тегов SDK. Команда версии —
`stmtool show-version`.

## Новое в v0.2.0

v0.2.0 — сквозной стабилизационный релиз: аудит багов драйверов/сенсоров/каркаса,
закрытие накопленного долга и перевод всей периферии на хелперы `reg::`.
Большинство изменений внутренние, но три **видны в исходниках** downstream-проектов.

### Ломающее: `SpiDataSize::Bits16` теперь отклоняется на этапе компиляции

Драйвер SPI STM32F4 всегда гонял регистр данных по одному байту, поэтому
`SpiDataSize::Bits16` давал битый фрейминг. `spi({...})` теперь отвергает его с
ошибкой компиляции. Если вы передавали `Bits16`, переключитесь на `Bits8`:

```cpp
Spi g_spi{*SPI2, spi({.clockHz = 10000000,
                      .mode = SpiMode::Mode0,
                      .lsbFirst = false,
                      .dataSize = SpiDataSize::Bits8})};  // было Bits16
```

### Ломающее: больше методов помечены `[[nodiscard]]`

Каждый возвращающий значение метод драйверов/RTOS/утилит теперь несёт
`[[nodiscard]]` (`CircularBuffer::{read,write,size,free_space,empty,full,
capacity}`, `reg::{read,get}`, статус-геттеры `DmaStream`, методы
`rtos::{Mutex,BinarySemaphore,Queue,Task}`, `clock::get*`). Под `-Werror` SDK
игнорирование такого возврата теперь не компилируется. Используйте значение или
пометьте отбрасывание явно:

```cpp
(void) uart.write(data);  // fire-and-forget теперь явный
```

### Ломающее: сеттеры диапазонов `IImu` возвращают `driver::Status`

`setAccelRange` / `setGyroRange` сменили тип с `void` на `driver::Status`, чтобы
NACK на шине во время `init()` больше не проглатывался. Концепт `sensor::IImu`
обновлён соответственно. Собственные реализации IMU должны обновить сигнатуры
обоих методов; `static_assert(IImu<...>)` в вашем драйвере это проверит на этапе
компиляции.

### Неломающие фиксы, о которых стоит знать

- **Клок GPIO** теперь включается для портов F–I (раньше только A–E), так что
  выводы этих портов работают на корпусах 100/144/176 пинов.
- **`postFromISR`** теперь делает yield на разбуженную более приоритетную задачу
  вместо ожидания до тика — меньше задержка ISR→задача для executor и шины сигналов.
- **Busy-wait периферии** (включение DMA, DMA-передача/TC UART) теперь ограничены
  и возвращают `Status::Timeout` вместо вечного спина на зависшей периферии.
- **UART DMA `writeNonBlocking`** теперь действительно возвращается сразу, а не
  блокирует на всю передачу.
- Весь MMIO в драйверах идёт через `driver::reg::*`; поведение побайтово идентично.

> Известный пробел: `system.work_queue` и `system.signal_bus` всё ещё тянут CMSIS
> / FreeRTOS, поэтому пока не покрыты host-тестами. Вынос портируемого шима
> critical-section запланирован на будущий релиз.

## Новое в v0.1.16

v0.1.16 завершает миграцию C→C++, начатую в v0.1.15: newlib-glue и CMSIS
`system_*` / исходники таблиц векторов теперь на C++. **Ломающих изменений нет,
при обновлении делать ничего не нужно** — имена символов, линковка и поведение
побайтово идентичны.

- **Ничего менять не надо.** `core/src/newlib/*.c`, `system_stm32f4xx.c` и
  per-family `vectors_*.c` стали `.cpp`, но `_start`, `_sbrk`, `_exit`,
  `SystemInit`, `Default_Handler`, каждый IRQ-хендлер и таблица `__isr_vectors`
  сохраняют своё `extern "C"` имя и линковку.
- **Weak-переопределения продолжают работать.** Переопределение `_sbrk`,
  `_write`, weak-IRQ-хендлера, `SystemInit` или `__assert_func` из приложения
  (`.cpp` или `.c`) перекрывает умолчание SDK ровно как раньше.
- **Вендорные заголовки остаются C-заголовками.** CMSIS device-заголовки
  (`stm32f4xx.h` и подтягиваемые им заголовки семейств) остаются `.h` и
  нетронутыми, чтобы быть diff-совместимыми с upstream ST; на `.cpp` переехали
  только `.c`-исходники рантайма.

## Новое в v0.1.15

v0.1.15 переводит собственный низкоуровневый рантайм-glue SDK с C на идиоматичный
C++. **Ломающих изменений нет, при обновлении делать ничего не нужно** — имена
символов, линковка и поведение побайтово идентичны.

- **Ничего менять не надо.** `trace.c`/`trace-impl.c`, `initialize-hardware.c`,
  `reset-hardware.c`, `exception-handlers.c` и `freertos_hooks.c` стали `.cpp`,
  но каждый символ, на который ссылаются таблица векторов, newlib-startup и ядро
  FreeRTOS, остаётся `extern "C"` с тем же именем.
- **Переопределения продолжают работать.** Если приложение переопределяет weak
  `__initialize_hardware`, `__initialize_hardware_early`, `__reset_hardware`,
  обработчик исключения или хук `vApplication*` — из `.cpp` или `.c` — оно
  по-прежнему перекрывает умолчание SDK ровно как раньше.
- **Вендорный код не тронут.** Файлы CMSIS `system_*`, таблицы векторов по
  семействам и newlib-рантайм остаются на C и нетронутыми.

## Новое в v0.1.14

v0.1.14 добавляет логирование и мультиарх-образ сборки. **Ломающих изменений
нет** — всё ниже опционально.

- **Логгер.** `import driver.log;` плюс `#include "driver/log.hpp"` дают макросы
  `LOG_*`; установите backend (`driver::log::ItmBackend` или
  `driver::log::UartBackend<UartDriver>`) один раз при старте. Настраивается
  cache-переменными `STM32_LOG_LEVEL` и `STM32_LOG_BACKEND`. Значения по умолчанию
  (`INFO` / `none`) ничего не выводят, пока вы не установите backend, поэтому
  существующие проекты не затронуты. См. [Логирование](modules/logging.md).
- **Apple Silicon: один `docker pull`.** Образ сборки теперь мультиарховый
  (`linux/amd64,linux/arm64`). Если у вас закэширован amd64-образ с v0.1.13,
  сделайте pull ещё раз, чтобы получить arm64-слой с нативным тулчейном — тогда
  `stmtool build` и `stmtool test` работают без падения под Rosetta. Флаги и CLI
  не меняются.

## Новое в v0.1.13

v0.1.13 — релиз **про качество и инфраструктуру**. Изменений публичного API SDK
нет, но сборочный workflow меняется ломающим образом:

- **`stmtool build --native` удалён.** Любая сборка теперь идёт внутри Docker-образа
  SDK; пути через хостовый тулчейн больше нет. Уберите `--native` из скриптов.
  Docker теперь обязателен. Если вы полагались на локальный `arm-none-eabi-gcc`,
  установите Docker — образ содержит запиненный тулчейн (15.2).
- **Артефакты переезжают из `build/` в `out/`.** `stmtool build` конфигурирует в
  `out/`, `stmtool flash` читает `.bin` из `out/`, `--clean` чистит `out/`.
  Добавьте `out/` в `.gitignore` проекта (шаблоны SDK уже это делают). Любые CI
  или скрипты, ссылавшиеся на `build/*.elf`, переведите на `out/*.elf`.
- **`stmtool test`** — новая команда: собирает и запускает host-юнит-тесты SDK в
  образе. Переиспользуемые мок-шины (`testing::MockI2c` / `MockSpi` / `MockUart` /
  `MockGpioPin` / `MockFlash`, модуль `testing.mock`) позволяют тестировать логику
  драйверов / сенсоров на хосте — см. документацию модуля тестирования.
- **Требуется GCC >= 14.** `stm32_sdk.cmake` прерывает конфигурацию, если ARM-компилятор
  старше GCC 14 (сканирование модулей C++20 его требует). Внутри образа это всегда
  выполнено; `stmtool doctor` отмечает устаревший локальный тулчейн.
- **FreeRTOS-Kernel вшит в образ** и больше не клонируется при каждой сборке —
  сборка идёт офлайн. Вне Docker `stm32_rtos.cmake` по-прежнему уважает
  `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL` для локального чекаута.

## Новое в v0.1.12

v0.1.12 — релиз **про форматирование и инструменты**, без изменений поведения
исходников и API. В C++-коде вашего проекта менять нечего. На локальный workflow
влияют две вещи:

- **Стиль SDK теперь Google 2 пробела / 80 колонок** (`Cpp11BracedListStyle:
  true`). Если ваш проект переиспользует `.clang-format` из SDK — перепрогоните
  `clang-format -i`, ожидайте реверстку только по пробелам. Вендорные / сторонние
  файлы в проекте можно исключить своим `.clang-format-ignore` (нужен
  clang-format 18+).
- **Опциональные pre-commit хуки.** SDK теперь поставляет
  `.pre-commit-config.yaml` (clang-format запинен на 19.1.3 + Conventional
  Commits + `poe ci`). Чтобы включить те же проверки в своём проекте:

  ```bash
  pip install pre-commit
  pre-commit install --hook-type pre-commit --hook-type commit-msg
  ```

  Используйте clang-format 19 под запиненную версию; более старые мажоры могут
  верстать иначе.

## Новое в v0.1.11

v0.1.11 вводит compile-time валидацию конфигов I2C / SPI / UART. Это **ломающее
изменение** для любого кода, который создаёт `I2c`, `Spi` или `Uart<>`.
Добавления thread-safety (#57) и юнит-тестов (#58) чисто аддитивны — новые
заголовки, в существующем коде менять нечего.

### Ломающее: конфиги I2C / SPI / UART (#56)

Для этих драйверов изменилось три вещи:

1. **Оборачивайте конфиг в валидатор.** Конфиг теперь должен проходить через
   свободный `consteval`-валидатор `i2c()` / `spi()` / `uart()`, ровно как
   `gpio()` / `exti()`. Невалидный конфиг теперь ошибка компиляции, а не молча
   неверное поведение.
2. **Поля SPI / UART теперь строгие enum'ы**, а не голые целые: `.mode`,
   `.dataSize`, `.dataBits`, `.stopBits`.
3. **Тип конфига вынесен из реализации.** `I2c::Config` теперь свободный
   `driver::I2cConfig` (и аналогично `driver::SpiConfig` / `driver::UartConfig`),
   живущий в interface-модуле. Реализация больше не реэкспортирует interface,
   поэтому добавьте `import driver.i2c;` (соотв. `driver.spi` / `driver.uart`)
   рядом с `import driver.stm32f4.i2c;`.

Было:

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

Стало:

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

Соответствие полей для перехода на enum:

- `.mode = 0` → `.mode = SpiMode::Mode0` (аналогично `Mode1`..`Mode3`).
- `.dataSize = 8` → `.dataSize = SpiDataSize::Bits8` (`16` → `Bits16`).
- `.dataBits = 8` → `.dataBits = DataBits::Eight` (`9` → `Nine`).
- `.stopBits = 1` → `.stopBits = StopBits::One` (`2` → `Two`).

### Аддитивно: thread-safety и юнит-тесты (#57, #58)

Менять нечего. `util/thread_safety.hpp` (в `sdk/core/include/util/`) — чисто
препроцессорный заголовок: его макросы под GCC no-op и на кодоген не влияют.
`testing/unit_test.hpp` — header-only хелпер за флагом `STM32_USE_TESTING`
(INTERFACE-таргет `stm32_testing`); включайте, только если хотите юнит-тесты на
устройстве. См. новый шаблон `bare-metal/unit-test-demo`.

## Новое в v0.1.10

v0.1.10 достраивает слой конкурентности. Он **аддитивен** — правки для апгрейда
не нужны, все существующие шаблоны не изменились, кроме нового
`button-events-demo`.

Новая поверхность, всё опционально:

1. **Драйвер EXTI** (`STM32_USE_DRIVERS`): `import driver.exti; import
   driver.stm32f4.exti;`. Настройте линию через `exti({.line, .port, .trigger,
   .priority})`, привяжите колбэк `ExtiLine::bind<&T::method>(cfg, obj)` и
   проведите вектор: `extern "C" void EXTI0_IRQHandler() { obj.irqHandler(); }`.
   Для линий 5–9 / 10–15 (общие векторы) вызывайте `irqHandler()` каждого объекта
   линии. Если колбэк использует `postFromISR`, приоритет ISR должен быть
   численно ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 у F4).
2. **`system::Timer`** (`STM32_USE_SYSTEM` + FreeRTOS): `import system.timer;`.
   `Timer::bind<&T::method>(exec, obj)`, затем `start(ms)` / `startPeriodic(ms)` /
   `stop()`.
3. **Ring-каналы**: `system::Channel` теперь принимает опциональный третий
   шаблонный аргумент `RingDepth` (по умолчанию `1`). Существующий код
   `Channel<Event, MaxSubs>` не меняется; используйте `Channel<Event, MaxSubs, N>`,
   когда события нельзя коалесцировать.
4. **Задачи внутри компонента**: `rtos::Task` теперь default-конструируем с
   идемпотентным `create(...)`. Перенесите создание задачи из `main` в `onStart()`
   компонента, если хотите, чтобы он владел своим воркером.

Ни один существующий символ не сменил сигнатуру ломающим образом; добавления в
`Channel` и `rtos::Task` обратно совместимы. См.
[System](modules/system.ru.md#достройка-слоя-конкурентности-v0110) и
[Drivers](modules/drivers.ru.md#exti--driverstm32f4exti-v0110).

## Новое в v0.1.9

v0.1.9 добавляет опциональный слой конкурентности в `sdk/system/` — `WorkQueue`,
`SingleThreadExecutor` и типобезопасную шину сигналов. Он **аддитивен**: ничего
в вашем существующем проекте не ломается, и для апгрейда правки не нужны. Все
шаблоны в дереве байт-в-байт без изменений; новый шаблон `signal-bus-demo` — это
рабочий пример.

Чтобы задействовать его в своём проекте (нужны `STM32_USE_SYSTEM` + FreeRTOS):

1. `import system.work_queue;` для RTOS-free очереди и/или
   `import system.executor; import system.signal_bus;` для FreeRTOS-слоёв.
   Правок CMake сверх того, что уже даёт `STM32_USE_SYSTEM`, не нужно — executor
   и шина сигналов компилируются автоматически, когда включён FreeRTOS.
2. Дайте каждому компоненту, откладывающему работу, интрузивный член
   `system::WorkItem` (`WorkItem::bind<&T::method>(*this)`) и ставьте его в общий
   `system::SingleThreadExecutor`.
3. Развязывайте компоненты через `system::Channel<Event, MaxSubs>`: издатели
   зовут `publish`, подписчики — `subscribe<&T::handler>(*this)` (обычно в
   `onBind()`). См. [System](modules/system.md#concurrency-layer-v019).

Ничто здесь не меняет API компонентов v0.1.8 — слой конкурентности стоит рядом с
ним. Если вы не импортируете новые модули, ваша сборка не затронута.

## Новое в v0.1.8

v0.1.8 добавляет опциональный каркас компонентов `sdk/system/`. Он
**аддитивен**: ничего в вашем существующем проекте не ломается, и для апгрейда
правки не нужны. Все встроенные шаблоны, кроме `imu-flash-oled-demo`, не
изменились байт-в-байт.

Если хотите внедрить каркас в своём проекте:

1. Включите в `CMakeLists.txt`: `set(STM32_USE_SYSTEM ON ...)` (требует
   `STM32_USE_DRIVERS`) и добавьте `stm32_system` в `target_link_libraries`.
2. `import system.component;` / `import system.bootstrap;`.
3. Смоделируйте `system::Component` — унаследуйте `system::ComponentBase` и
   реализуйте `onRegister/onInit/onBind/onStart`; завершите
   `static_assert(system::Component<Foo>)`.
4. Соберите граф в composition-root `struct` и запустите через
   `system::bootstrap(...)`. См. [System](modules/system.md).

Шаблон `imu-flash-oled-demo` переписан на каркасе. Если вы копировали старую
версию этого демо (свободные задачи + глобалы), она по-прежнему собирается с
SDK — переход на компоненты не навязывается. Новая версия — референс того, как
части складываются вместе.

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

**Стало (latest):**

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
Uart<512, 256, UartMode::Dma> g_uart2{...};
```

## Новые доступные модули

После апгрейда становятся доступны (без дополнительной настройки):

- `driver.stm32f4.dma` — `DmaStream` RAII-обёртка + таблица `dmaMap`.
- `sensor.display` — интерфейс `IDisplay`.
- `sensor.ssd1306` — драйвер SSD1306 OLED.
- `sensor.external_flash` — интерфейс `IExternalFlash`.
- `sensor.w25q32` — драйвер SPI flash W25Q32.

## Изменения в clang-format

Начиная с v0.1.12 стиль — Google со следующими отклонениями:

- `IndentWidth` / `TabWidth`: 2, `ColumnLimit`: 80, `AccessModifierOffset`: -2
- `BinPackArguments: false`, `BinPackParameters: false`
- `AllowAllArgumentsOnNextLine: false`, `AllowAllParametersOfDeclarationOnNextLine: false`
- `AlignAfterOpenBracket: BlockIndent`
- `Cpp11BracedListStyle: true` (braced-init списки как `{nullptr}`)

Вендорные CMSIS / newlib исходники исключены через `.clang-format-ignore`
(clang-format 18+). При `clang-format -i` после апгрейда — ожидайте шумные diff'ы
на call-сайтах с aggregate-конфигами. Используйте trailing comma после последнего
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
