# Релизы

## Политика версионирования

Теги используют SemVer (`vMAJOR.MINOR.PATCH`). Проект находится в pre-1.0:
и patch-, и minor-релизы могут включать небольшие изменения, требующие правок
в пользовательском коде. Где именно проходит граница «patch vs minor» — решает
maintainer, пока API не стабилизируется (цель — v1.0).

Практические рекомендации для подключающих SDK проектов:

- Фиксируйте конкретный тег в `stmproject.toml` (`[sdk] version = "0.1.2"`),
  а не `develop`.
- Перед апгрейдом — прочитайте [заметки по апгрейду](migration.md).
- Используйте `stmtool sdk update --version <тег>` для обновления кэша.

## Инструменты

Номер версии выводится из git-тегов через `poetry-dynamic-versioning`
(переход с `setuptools-scm` в v0.1.4). Нигде нет ручной константы —
именно тег создаёт релиз для пользователей `stmtool`.

`tools/stmtool/pyproject.toml` конфигурирует:

```toml
[build-system]
requires = ["poetry-core>=1.9", "poetry-dynamic-versioning>=1.4"]
build-backend = "poetry_dynamic_versioning.backend"

[tool.poetry-dynamic-versioning]
enable = true
vcs = "git"
style = "pep440"
```

Плагин поднимается от `tools/stmtool/pyproject.toml` до корневого
`.git` и берёт последний `vMAJOR.MINOR.PATCH` тег — тот же источник
истины, что использует и CMake-сторона SDK.

## Процедура релиза

1. `develop` должен быть зелёным на CI (matrix F407VG/F401CE/F411CE).
2. Локально проверить доки: `mkdocs serve`.
3. Повесить тег на merge-коммит в `develop`:
   ```bash
   git tag -a v0.1.3 -m "Release v0.1.3" <коммит>
   git push origin v0.1.3
   ```
4. Создать GitHub Release с заметками из секции «История релизов» этой страницы.
5. Workflow `docs.yml` пересоберёт сайт автоматически — проверить, что
   <https://khosta77.github.io/stm32-sdk/> подтянул свежий контент.

## История релизов

### v0.2.0

Фокус: сквозной стабилизационный проход, закрывающий линейку v0.1.x — аудит
багов драйверов/сенсоров/каркаса, закрытие накопленного долга и унификация
доступа к MMIO. Открывает серию v0.2.x (issue #74).

Основное:

- **Фиксы корректности.** Включение клока GPIO теперь охватывает порты F–I (было
  только A–E); `SingleThreadExecutor::postFromISR` делает yield на разбуженную
  более приоритетную задачу вместо ожидания тика; `Mpu6050::init` больше не
  проглатывает статус записи диапазонов accel/gyro; busy-wait DMA/UART ограничены
  и возвращают `Status::Timeout`; UART DMA `writeNonBlocking` действительно
  неблокирующий; `SpiDataSize::Bits16` (никогда не реализованный) отклоняется на
  этапе компиляции; таймаут I2C `BTF` рапортуется вместо ложного `Ok`.
- **Унификация MMIO.** Каждый доступ к регистрам в драйверах GPIO/I2C/SPI/flash/
  clock идёт через `driver::reg::*` — сырых `|=` / `=` по `volatile`-регистрам не
  осталось. Поведение побайтово идентично.
- **`[[nodiscard]]` повсюду.** Все возвращающие значение методы драйверов/RTOS/
  утилит теперь `[[nodiscard]]`, так что игнорирование статусов роняет сборку под
  `-Werror`.
- **Покрытие host-тестами.** Новые `ctest`-исполняемые для логики `CircularBuffer`,
  `W25q32` и `Ssd1306` на мок-шинах (всего 7 host-тестов).
- **Чистка шаблонов и инструментов.** Шаблон `bare-metal/blink` теперь использует
  драйвер `GpioPin` вместо сырого MMIO; удалённый флаг сборки `--native` убран из
  всех `CLAUDE.md` шаблонов и build-скиллов репозитория.

Замечания:

- Три видимых в исходниках ломающих изменения: `SpiDataSize::Bits16` больше не
  компилируется, новые пометки `[[nodiscard]]` могут задеть downstream `-Werror`,
  а `IImu::setAccelRange` / `setGyroRange` теперь возвращают `driver::Status`. См.
  [заметки по обновлению](migration.ru.md#новое-в-v020).
- Отложено: SDK-wide оптимизация размера `-fno-exceptions` / `-fno-rtti` вынесена
  отдельно — она конфликтует с `throw`-валидаторами конфигов на `consteval` и
  требует переписывания на throw-free идиому.

### v0.1.16

Фокус: завершение миграции C→C++ — на C++ переезжают последние вендорные
рантайм-исходники: newlib-glue и CMSIS `system_*` / таблицы векторов, без
изменения ABI и поведения (issues #42, #43). Это закрывает линейку v0.1.x.

Основное:

- **newlib-glue → C++ (#42).** `core/src/newlib/{assert,exit,sbrk,startup,
  syscalls}.cpp` заменяют прежние `.c`. Каждый C-ABI символ, на который
  ссылаются линкер и libc — `_start`, `_sbrk`, `_exit`, `abort`,
  `__assert_func`, `__initialize_args`, стабы libnosys/POSIX — остаётся
  `extern "C"` с тем же именем; `_start` в `.after_vectors`, `always_inline`
  хелперы копирования/обнуления, привязка `register char* asm("sp")` и контракт
  `-nostartfiles` / `nano.specs` сохранены дословно. `main` объявлен вне блока
  `extern "C"` (он никогда не манглится и не может иметь C-линковку).
- **CMSIS `system_*` / таблицы векторов → C++ (#43).** `system_stm32f4xx.cpp` и
  семь per-family `vectors_*.cpp` компилируются как C++. Таблицы векторов,
  `Default_Handler` и каждый weak-хендлер с `alias("Default_Handler")` обёрнуты
  в `extern "C"`, чтобы имена резолвились немангленными; `__isr_vectors`
  возвращён к внешней линковке (иначе `const`-массив стал бы TU-local в C++) с
  сохранением размещения в `.isr_vector`. `SystemInit` / `SystemCoreClockUpdate`
  наследуют C-линковку из CMSIS-заголовка. Регистровая логика ST не тронута.

Замечания:

- Ломающих изменений нет. Имена символов, линковка и поведение побайтово
  идентичны — проверено `nm` (нет мангленных C-ABI символов) и неизменным
  размером flash во всех десяти шаблонах. Мигрированные исходники теперь
  форматируются как остальной SDK; только вендорные CMSIS device-заголовки
  (`stm32f4xx.h` и их паутина include) остаются `.h` и нетронутыми — их
  переименование сломало бы внутренние include ST и diff против upstream. См.
  [заметки по обновлению](migration.md#new-in-v0116).
- Инструментарий: `stmtool flash --verify` больше не передаёт невалидный флаг в
  `st-flash` 1.8.0 (который и так сверяет каждую запись), а вендорные
  CMSIS-заголовки помечены `linguist-vendored`, чтобы статистика языков GitHub
  отражала авторский C++, а не ~10 МБ заголовков ARM/ST.

### v0.1.15

Фокус: перевод собственного низкоуровневого C-glue SDK на идиоматичный C++, без
изменения ABI и поведения (issues #38, #39, #40, #41).

Основное:

- **Диагностический trace-glue → C++ (#38).** `core/src/diag/trace.cpp` и
  `trace-impl.cpp` заменяют прежние `.c`: `constexpr` для размера буфера,
  `static_cast`/`reinterpret_cast`, файл-локальные backend'ы в анонимном
  namespace. Переносимые `trace_*` сохраняют C-линковку через `diag/trace.h`.
- **Glue инициализации/сброса → C++ (#39).** `initialize-hardware.cpp` и
  `reset-hardware.cpp` оборачивают `__initialize_hardware_early` /
  `__initialize_hardware` / `__reset_hardware` в `extern "C"`, чтобы остающиеся
  C `_start` / `_exit` из newlib резолвили их без изменений; `weak`-переопределения
  и доступ к CMSIS `SCB->` сохранены дословно.
- **Обработчики исключений → C++ (#40).** `exception-handlers.cpp` держит все
  `Reset_Handler` / `HardFault_Handler` / `SysTick_Handler` … как `extern "C"`
  (вендорная таблица векторов ссылается на них по имени), с целыми naked-asm-
  трамплинами, секциями `.after_vectors` и диагностикой `TRACE`/`DEBUG`.
- **Хуки FreeRTOS → C++ (#41).** `rtos/src/freertos_hooks.cpp` держит
  `vApplicationStackOverflowHook` и прочие как `extern "C"` для ядра; статическое
  хранилище idle/timer-задач переезжает в анонимный namespace.

Замечания:

- Ломающих изменений нет. Имена символов и поведение побайтово идентичны —
  переопределение `__initialize_hardware`, `__reset_hardware` или любого
  обработчика из кода приложения (в том числе из `.c`-файла) по-прежнему работает
  через границу `extern "C"` + `weak`. Мигрированные файлы теперь форматируются
  как остальной SDK; вендорные `system_*` / таблицы векторов и newlib-рантайм
  остаются на C. См. [заметки по обновлению](migration.md#new-in-v0115).

### v0.1.14

Фокус: логирование с фильтрацией на этапе компиляции и сменными backend'ами, плюс
мультиарх-образ сборки, чтобы локальные сборки шли нативно на Apple Silicon
(issues #36, #37, #81).

Основное:

- **Compile-time логирование `LOG_*` (#36).** Новый модуль `driver.log` и текстовый
  хедер макросов `driver/log.hpp` дают `LOG_ERROR`/`WARN`/`INFO`/`DEBUG`/`TRACE`,
  каждый в формах bare / `_U32` / `_HEX`. Два фильтра: compile-time потолок
  `STM32_LOG_LEVEL` раскрывает вырезанные уровни в `((void)0)` (вместе со
  строкой-форматом — ноль flash), а runtime `driver::log::setLevel` фильтрует
  остальное без пересборки. Форматирование вручную, поэтому работает под
  `nano.specs`.
- **Сменные backend'ы (#37).** Sink — это captureless-thunk
  `void(*)(void*, const char*, size_t)`, устанавливаемый через
  `driver::log::setSink`. Два поставляются с `install()`: `ItmBackend` (SWO/ITM
  port 0, читается по SWV) и `UartBackend<UartDriver>` (обобщён по любому
  `driver::IUart`). `STM32_LOG_BACKEND` (`none`/`itm`/`uart`) называет
  предполагаемый на этапе сборки и задаёт define `STM32_LOG_BACKEND` для
  обобщённого кода приложения.
- **Мультиарх-образ сборки (#81).** `docker/Dockerfile.build` и `.ci` выбирают
  ARM-тулчейн по архитектуре хоста (`uname -m`), а `docker-image.yml` публикует
  манифест `linux/amd64,linux/arm64`. На Apple Silicon `docker pull` теперь тянет
  arm64-слой с нативным `aarch64`-тулчейном, поэтому `stmtool build` / `stmtool
  test` больше не падают под Rosetta. Пользовательский контракт не меняется —
  только образ, без новых флагов.

Замечания:

- Ломающих изменений нет. Логирование опциональное и поставляется с
  `STM32_USE_DRIVERS`; значения по умолчанию (`STM32_LOG_LEVEL=INFO`,
  `STM32_LOG_BACKEND=none`) ничего не добавляют, пока вы не установите backend.
  См. [Логирование](modules/logging.md) и [заметки по обновлению](migration.md#new-in-v0114).
- Пользователям Apple Silicon с закэшированным amd64-образом стоит один раз
  сделать `docker pull`, чтобы заменить его на мультиарх-манифест.

### v0.1.13

Фокус: релиз про качество и инфраструктуру — воспроизводимый тулчейн, реальный
путь host-тестов и удаление расходящейся локальной сборки (issues #33, #34, #35,
#47, #79).

Основное:

- **Сборка только в Docker; артефакты в `out/`.** `stmtool build --native`
  удалён; любая сборка идёт внутри образа SDK, поэтому окружение одинаково локально
  и в CI. Вывод сборки переехал из `build/` в `out/` (в gitignore). Образ
  переопределяется через `STMTOOL_DOCKER_IMAGE`. **Ломающее** — уберите `--native`
  и переведите скрипты с `build/*.elf` на `out/`.
- **Запиненный тулчейн, единый источник правды (#33).** Образ поднимает
  `arm-none-eabi-gcc` до 15.2 (пин 13.2 молча не умел сканировать импорты модулей
  C++20), а `stm32_sdk.cmake` теперь прерывает конфигурацию ниже GCC 14. `stmtool
  doctor` парсит версию компилятора, отмечает устаревший тулчейн и сообщает,
  присутствует ли SDK-образ.
- **Переиспользуемые мок-шины (#34).** Новый модуль `testing.mock` поставляет
  `testing::MockI2c` / `MockSpi` / `MockUart` / `MockGpioPin` / `MockFlash` —
  обычные struct, удовлетворяющие концептам драйверов, программируемые
  скриптованными ответами и захватом записи, так что логика драйверов / сенсоров
  тестируется без железа.
- **Host-юнит-тесты + `stmtool test` (#35).** `tests/host/` собирает CMSIS-free
  переносимый слой хостовым `g++` из образа и запускает `ctest` — инварианты
  `Result<T>` / `DRV_TRY`, моки и пример MPU6050-на-моке. `stmtool test` гоняет
  набор в Docker; job `host-tests` в CI выполняет ту же команду.
- **FreeRTOS вшит в образ (#47).** Ядро (`V11.1.0`) клонируется один раз при
  сборке образа и подхватывается через `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL`,
  поэтому FreeRTOS-шаблоны больше не клонируют его для каждого проекта и собираются
  офлайн.
- **CI на образе.** `build.yml` убирает action ARM-тулчейна, прогревает общий
  buildx-кэш и грузит образ из него (герметично — без зависимости от
  опубликованного тега), а новый `docker-image.yml` публикует образ в GHCR.

Замечания:

- Изменений C++-API SDK нет; ломается сборочный workflow (`--native`, `out/`,
  требование Docker). См. [заметки по обновлению](migration.md#new-in-v0113).
- Host-тестам нужен GCC 15 (g++-14 некорректно компилирует модули, включающие
  `<cstddef>` в global module fragment); образ ставит его из PPA тулчейна.

### v0.1.12

Фокус: единый принудительный стиль кода. Одна конфигурация clang-format,
прогнанная по всему дереву, плюс обязательные pre-commit хуки, чтобы стиль и
история коммитов больше не разъезжались (issues #59, #60).

Основное:

- **Единый стиль clang-format, Google 2 пробела / 80 колонок.** `.clang-format`
  переведён на ближайшие к нему дефолты Google: `IndentWidth` / `TabWidth` 2,
  `ColumnLimit` 80, `AccessModifierOffset` -2 и — собственно баг из #59 —
  `Cpp11BracedListStyle: true`. Прежний явный `Cpp11BracedListStyle: false`
  противоречил коду, который написан `{nullptr}` (без внутренних пробелов), и
  разъезжался с clang-format 19; теперь braced-init списки форматируются
  по-Google'овски. Весь SDK и все десять шаблонов переформатированы одним
  механическим прогоном — без изменений поведения.
- **Вендорные исходники вне форматтера.** Новый `.clang-format-ignore` исключает
  сторонние CMSIS core / device заголовки, newlib / Cortex-M runtime-glue и
  сгенерированную таблицу шрифта — они остаются pristine и diff-абельными с
  upstream. В clang-format `*` матчит в пределах одного сегмента пути, поэтому
  глубина в шаблонах прописана явно.
- **Обязательные pre-commit хуки.** Новый `.pre-commit-config.yaml` включает три
  проверки: `clang-format` запинен на 19.1.3 (чтобы хук, CI и дерево совпадали
  байт-в-байт) по исходникам C / C++ / `.cppm`, `conventional-pre-commit` на
  сообщении коммита и pre-push прогон `poe ci`, ограниченный `tools/stmtool`.
  Установка один раз: `pip install pre-commit && pre-commit install --hook-type
  pre-commit --hook-type commit-msg`.
- **CI-гейт формата.** Новый workflow `Format` гоняет тот же запиненный
  clang-format хук на каждый push и PR в `develop`, так что разъехавшийся стиль
  падает в CI, а не проходит.
- **Гайд для контрибьюторов.** Новый `CONTRIBUTING.md` описывает процесс:
  установка хуков, английские Conventional Commits, русские описания PR, `poe ci`
  для `stmtool`.

Примечания:

- Ни поведение исходников, ни API не меняются — релиз только про форматирование и
  инструменты. Существующим downstream-проектам правки кода не нужны; повторный
  прогон `clang-format -i` с новым конфигом переверстает код в 2 пробела / 80
  колонок.
- `.clang-format-ignore` требует clang-format 18+; запиненный хук использует
  19.1.3.

### v0.1.11

Фокус: качество и типобезопасность — `consteval`-валидаторы конфигов I2C / SPI /
UART, аннотации thread-safety и хелперы юнит-тестов на устройстве
(issues #56–#58).

Основное:

- **`consteval`-валидаторы конфигов `i2c()` / `spi()` / `uart()`.** Раньше
  compile-time валидацию имели только `GpioConfig` / `ExtiConfig`. Теперь конфиги
  I2C / SPI / UART — свободные структуры в interface-модулях (`driver::I2cConfig`,
  `driver::SpiConfig`, `driver::UartConfig`), вынесенные из типов реализации (было
  `I2c::Config`), рядом со свободными `consteval`-валидаторами (`driver::i2c()` /
  `driver::spi()` / `driver::uart()`) в том же стиле, что `gpio()` / `exti()`.
  Поля SPI / UART стали строгими enum'ами с сентинелом `None`: `SpiMode`
  (`Mode0`..`Mode3`), `SpiDataSize` (`Bits8` / `Bits16`), `DataBits` (`Eight` /
  `Nine`), `StopBits` (`One` / `Two`). Невалидный конфиг больше не компилируется.
  UART `Config`, ранее продублированный в основном шаблоне и DMA-специализации,
  теперь единый.
- **Аннотации thread-safety.** Новый чисто-препроцессорный заголовок
  `util/thread_safety.hpp` (в `sdk/core/include/util/`) оборачивает атрибуты
  thread-safety из Clang: `CAPABILITY`, `SCOPED_CAPABILITY`, `GUARDED_BY`,
  `REQUIRES`, `ACQUIRE`, `RELEASE`, `EXCLUDES`, `NO_THREAD_SAFETY_ANALYSIS` и др.
  Под Clang (`-Wthread-safety`) они дают статанализ; под GCC (на котором собирается
  SDK) — no-op. Размечены: `rtos::Mutex` (`CAPABILITY`) и `rtos::LockGuard`
  (`SCOPED_CAPABILITY`) в `rtos.hpp`; PRIMASK-критсекция смоделирована как
  именованная capability (`system::detail::g_criticalSection`), `enterCritical` /
  `leaveCritical` как `ACQUIRE` / `RELEASE`, а `WorkQueue::_head` / `Channel::_ring`
  как `GUARDED_BY`. Это groundwork — реальный прогон анализа Clang приедет с
  clang-tidy в CI (#73); сейчас важно, что всё компилируется чисто под GCC
  `-Werror` и кодоген не меняется (макросы разворачиваются в пусто).
- **Хелперы юнит-тестов на устройстве.** Новый header-only заголовок
  `testing/unit_test.hpp` (в `sdk/testing/include/testing/`, INTERFACE-таргет
  `stm32_testing`, флаг `STM32_USE_TESTING`). GTest-подобные `ASSERT_*` /
  `EXPECT_*` без исключений, без кучи, без динамической линковки: `ASSERT_` делает
  `return` из тест-функции, `EXPECT_` продолжает. `TEST(name){...}` определяет тест
  как функцию; `TestRunner runner{writer}` принимает инжектируемый `Writer`
  (`void(*)(const char*)`); `RUN_TEST(runner, name)` гоняет тест; `runner.summary()`
  печатает «N passed, M failed» и возвращает bool. Форматирование целых — вручную
  (без `snprintf`), поэтому один и тот же код тестов компилируется и гоняется и на
  хосте (writer → stdout), и на устройстве (writer → UART). Новый шаблон
  `bare-metal/unit-test-demo` демонстрирует это (тесты на `driver::Result<T>`).
  Дополняет будущие host-тесты (#34 / #35).

Примечания:

- **Ломающее для вызовов драйверов.** Конфиги I2C / SPI / UART теперь надо
  оборачивать в валидатор (`I2c g{*I2C1, i2c({...})}`), поля SPI / UART стали
  enum'ами, а тип конфига переехал (`I2c::Config` → `driver::I2cConfig`, требует
  `import driver.i2c;`). См. [миграцию](migration.ru.md#new-in-v0111).
- Добавления thread-safety и тестов чисто аддитивны — новые заголовки, для
  существующего кода правки не нужны.

### v0.1.10

Фокус: достройка слоя конкурентности — драйвер EXTI, software `Timer`,
компоненты со своей задачей и ring-режим канала сигналов (issues #52–#55).

Основное:

- Новый **драйвер EXTI** `driver.stm32f4.exti` (+ концепт `driver::IExti`,
  агрегат `ExtiConfig` и `consteval`-валидатор `exti({...})`). Маршрутизирует
  вывод GPIO на линию EXTI, настраивает фронт и вызывает captureless-колбэк из
  `irqHandler()`. Только CMSIS, весь MMIO через `reg::*`. В паре с
  `executor.postFromISR` даёт классический вынос работы из ISR.
- Новый **`system::Timer`** (`system.timer`, только под FreeRTOS): тонкая
  типобезопасная обёртка над `postAfter` / `addPeriodic` / `cancel` executor'а.
  Колбэк-thunk, без `std::function`. `start` / `startPeriodic` / `stop` /
  `active`.
- У **`Channel<Event, MaxSubs, RingDepth = 1>`** появился ring-режим. Дефолт —
  коалесцирующий слот из v0.1.9; `RingDepth > 1` доставляет каждое событие в
  порядке FIFO (drop-oldest при переполнении, без кучи). Порядок проверяется
  `consteval`-самотестом на отдельном `detail::EventRing`.
- **Deferred-start `rtos::Task`**: default-конструируем плюс идемпотентный
  `create(...)`, так что компонент может владеть своей задачей и стартовать её в
  `onStart()`, а не в `main`.
- Новый шаблон `button-events-demo`: EXTI-кнопка → `postFromISR` → дебаунс
  `Timer` → ring-`Channel` → LED, плюс компонент `Heartbeat` со своей
  deferred-start задачей.

Примечания:

- **Аддитивно** — драйвер EXTI собирается с `STM32_USE_DRIVERS`; `Timer` следует
  за executor под `STM32_USE_FREERTOS`. Правка `Channel` совместима по исходникам
  (новый хвостовой шаблонный параметр с дефолтом = старое поведение), а новые
  члены `rtos::Task` не трогают существующие вызовы. См.
  [миграцию](migration.ru.md#new-in-v0110).
- Приоритет ISR EXTI должен быть численно ≥
  `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 у F4), чтобы вызывать
  `postFromISR`.

### v0.1.9

Фокус: zero-cost слой конкурентности поверх каркаса компонентов — очередь работ,
однопоточный executor, типобезопасная шина сигналов (issues #30–#32).

Главные изменения:

- Три новых модуля в `stm32_system`: `system.work_queue`, `system.executor`,
  `system.signal_bus`. Тот же стиль zero-vtable / zero-heap, что в v0.1.7/v0.1.8
  — вызовы это thunk'и `void(*)(void*)`, а не `std::function`.
- `WorkQueue` + `WorkItem`: **интрузивная очередь отложенной работы, которой
  владеет клиент** (без кучи). `schedule` / `schedulePriority` / `scheduleAfter`
  / `cancel`, `runDue(now)` для диспетчеризации по тикам и `runOnce()` для
  bare-metal super-loop. Постановка идемпотентна; порядок проверяется
  `consteval` self-check'ом.
- `SingleThreadExecutor`: связывает `WorkQueue` с одной `rtos::Task` + семафором
  пробуждения. Обработчики бегут **последовательно на одной задаче** (без
  мьютекса на обработчик); `post` / `postAfter` / `addPeriodic` / `postFromISR`.
- `Channel<Event, MaxSubs>`: **типобезопасный** pub/sub — событие это тривиально
  копируемая тег-структура, подписчики в фикс-массиве (без кучи), веерная
  рассылка на executor, поэтому подписчики не гоняются. Сознательно расходится с
  референсом: без строковых имён издателей, без `reinterpret_cast`.
- Новый шаблон `signal-bus-demo`: `Producer` (своя задача) публикует, а
  реактивный `Consumer` (без собственной задачи) обрабатывает события на
  executor, подписавшись в `onBind()`.

Примечания:

- **Аддитивно** — слой opt-in. Ядро очереди RTOS-free (только CMSIS PRIMASK) и
  строится всегда; executor и шина сигналов компилируются только под
  `STM32_USE_FREERTOS`. Все существующие шаблоны без изменений и зелёные. См.
  [миграцию](migration.md#new-in-v019).
- Без `<tuple>`, без кучи, без runtime-полиморфизма. `system.work_queue` зависит
  только от CMSIS, что оставляет ядро очереди host-тестируемым на будущее.

### v0.1.8

Фокус: zero-cost каркас приложения — жизненный цикл компонентов, соглашение DI,
composition root (issues #26–#29).

Главные изменения:

- Новый слой `sdk/system/`, включается флагом `STM32_USE_SYSTEM` (требует
  `STM32_USE_DRIVERS`), линкуется как `stm32_system`. Два модуля:
  `system.component` и `system.bootstrap`.
- `system::Component` — это **C++20-концепт** (не виртуальный базовый класс):
  компонент предоставляет `onRegister/onInit/onBind/onStart` (каждый возвращает
  `driver::Status`) плюс аксессоры состояния/критичности/имени — последние
  бесплатно из невиртуальной примеси `system::ComponentBase`. Ноль vtable, в
  духе концепт-миграции v0.1.7.
- Фиксированные фазы `Register → Init → Bind → Start`, прогоняемые барьером
  через `system::bootstrap(components...)`, с **критичностью**: отказ `Critical`
  останавливает bootstrap; отказ `Common` считается и пропускается, а система
  продолжает деградированно. `bootstrap` возвращает `BootReport`
  (`status`, `failedComponent`, `failedPhase`, `degraded`).
- Соглашение DI `Config` + `Environment`: compile-time константы против
  переданных ссылками зависимостей; компонент, которому нужна обобщённая шина, —
  ограниченный шаблон (`template <driver::II2c I2cDriver>`).
- Composition root как `struct`: граф зависимостей — это список членов
  (конструируются в порядке объявления); единственный список аргументов
  `bootstrap(...)` — единственный источник правды о наборе компонентов, без
  ручного списка регистрации.
- Шаблон `imu-flash-oled-demo` переписан на каркасе
  (`ImuSampler` + `DisplayView` + `FlashLogger`) как рабочий пример.

Примечания:

- **Аддитивно** — каркас opt-in. Все остальные шаблоны без изменений и остаются
  зелёными; на компоненты переехал только `imu-flash-oled-demo`. См.
  [миграцию](migration.md#new-in-v018).
- Без `<tuple>`, без кучи, без runtime-полиморфизма — `bootstrap` это
  вариадик-свёртка по паку компонентов. Модули `system.*` зависят только от
  `driver.types`, что оставляет их host-тестируемыми на будущее.

### v0.1.7

Фокус: миграция интерфейсов драйверов и сенсоров с виртуальных базовых
классов на **C++20-концепты** — без vtable на вызовах шины (issues #17–#25).

Главные изменения:

- Интерфейсы драйверов `IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash` теперь
  концепты (`interface/i_*.cppm`); `GpioPin`, `I2c`, `Spi`, `Uart`,
  `InternalFlash` теряют наследование и `override`, каждый защищён self-check'ом
  `static_assert(IXxx<Impl>)`. `NullGpioPin` теперь обычный `struct`.
- Интерфейсы сенсоров `IImu`, `IDisplay`, `IExternalFlash` теперь концепты;
  `Mpu6050`, `Ssd1306`, `W25q32` становятся шаблонами класса, параметризованными
  шиной (`template <driver::II2c I2cDriver> class Mpu6050`, …). Вызовы шины прямые.
  CTAD сохраняет создание без изменений (`sensor::Mpu6050 g{i2c, {...}}`).
  Каждый модуль сенсора несёт compile-time self-check `static_assert` против
  тривиальной mock-шины.
- Константы устройства `W25q32` переехали в нешаблонный `sensor::W25q32Spec`
  (`CAPACITY`, `SECTOR_SIZE`, `PAGE_SIZE`, `JEDEC_W25Q32JV`).
- Каждый метод драйвера/сенсора, возвращающий значение, теперь `[[nodiscard]]`
  (не только тип `Status`): проигнорировать счётчик байт `Uart::write` или
  геометрию флеша — ошибка сборки. Fire-and-forget записи в UART используют
  явный `(void)`.
- Латентные баги, обнажённые шаблонизацией, исправлены: `Mpu6050::setAccelRange`
  / `setGyroRange` игнорировали `[[nodiscard]] Status`; delay-петля в init
  использовала deprecated-инкремент `volatile`; `Ssd1306::sendCommands`
  применял range-for по `std::span`, который GCC 15 не разрешает через границу
  модуля в шаблонном методе (заменён на индексную петлю).

Примечания:

- Несовместимость по исходникам — только для кода, который называл
  интерфейсный тип напрямую (`II2c&`, `sensor::Mpu6050*`) или перемещённые
  константы `W25q32`. Все встроенные шаблоны используют конкретные типы и
  потребовали лишь переименования в `W25q32Spec` и правок приведения
  указателей. См. [миграцию](migration.md#upgrading-to-v017).
- Без runtime-полиморфизма / гетерогенных контейнеров шин (на одном таргете не
  нужно). CMake не меняется — имена файлов и модулей те же.

### v0.1.6

Фокус: только процесс и документация — изменений SDK/API нет.

Главные изменения:

- Процесс релиза теперь требует **двуязычных** GitHub Release notes: секция
  `## English` дословно из `docs/release.md` и секция `## Русский` из
  `docs/release.ru.md`, разделённые `---`, с футером, ссылающимся на страницу
  релизов в docs и changelog `vPREV...vNEW`. Закреплено в `CLAUDE.md`
  (Release process, шаг 4).

Примечания:

- Совместим по исходникам с v0.1.5; пересборка downstream-проектов не требуется.

### v0.1.5

Фокус: единый слой обработки ошибок с нулевым overhead для `driver.types`
([issue #15](https://github.com/khosta77/stm32-sdk/issues/15),
[issue #16](https://github.com/khosta77/stm32-sdk/issues/16)).

Главные изменения:

- `driver::Result<T>` — несёт либо значение `T`, либо ошибку `Status`
  (1-байтовый тег, без аллокаций, без исключений, полностью `constexpr`).
  Помечен `[[nodiscard]]`; `T` должен быть тривиально разрушаемым. Методы:
  `ok()`, `status()`, `value()` (предусловие `ok()`), `valueOr()`,
  `operator bool`.
- `DRV_TRY(expr)` и `DRV_TRY_ASSIGN(var, expr)` — ранний возврат в стиле
  Rust-`?`, в текстовом заголовке `driver/try.hpp` (макросы не экспортируются
  модулем C++20; подключать после `import driver.types;`).
- `driver::Status` теперь `[[nodiscard]]`. Под обязательным `-Werror` это
  превращает любую проигнорированную ошибку в ошибку сборки. Встроенные
  шаблоны обновлены соответственно: явный `(void)` на намеренном сбросе байта
  в UART RX ISR и реальные проверки вокруг `Mpu6050::init()` и
  `W25q32::eraseSector()` в FreeRTOS-демо.

Примечания:

- Host-юнит-тест из #15/#16 перенесён на v0.1.10, где появляется полный
  host-test-каркас (мок-шины #34, host-test CI job #35). Пока инварианты
  `Result<T>` проверяются `consteval`-самопроверкой внутри `driver.types` на
  этапе компиляции в каждом ARM-билде.

### v0.1.4

Фокус: качество и инфраструктура, новых SDK-фич нет кроме
[issue #9](https://github.com/khosta77/stm32-sdk/issues/9).

Главные изменения:

- `driver::NullGpioPin` — пустая реализация `IGpioPin` для плат, где
  CS-линия SPI впаяна в GND (issue #9). Drop-in для сенсоров,
  принимающих `IGpioPin&` (например, `W25q32`).
- `stmtool` мигрирован с `setuptools` на Poetry + `poethepoet`.
  Новая команда `poetry run poe ci` запускает ruff / flake8 / pylint
  / black / isort / mypy (strict) / bandit / pytest за один заход.
  `poetry run poe fix` применяет все авто-фиксеры. Рантайм `__version__`
  теперь берётся через `importlib.metadata`; генерируемый `_version.py`
  удалён.
- У `stmtool` появился baseline тест-сьют (~80% покрытия, 50 тестов)
  и новый workflow `.github/workflows/stmtool.yml`, запускающий
  `poe ci` на каждом PR, затрагивающем `tools/stmtool/**`.
- `.github/workflows/build.yml` переписан: chip-matrix теперь
  `[STM32F407VG]`, и каждый PR собирает **все 7 шаблонов** параллельно
  (`fail-fast: false`). ARM-тулчейн в CI поднят до `15.2.Rel1`: GCC 13
  не умеет сканировать импорты модулей C++20 (`-fdeps-format=p1689r5`
  есть только с GCC 14), поэтому любой шаблон, линкующий `stm32_drivers`,
  требует GCC >= 14. Документированный минимум теперь 14.
- `-Wall -Wextra -Wpedantic -Wshadow -Werror` постоянно включены на
  `stm32_core` и пропагируются на драйверы, RTOS, сенсоры, пользовательский
  код. Политика — на странице [Compiler and warning flags](build-flags.md).
- Project-level `.claude/commands/{ci,fix,build-all-templates,test-template,release-check}.md`
  для контрибьюторов, использующих Claude Code.
- Новая секция `CLAUDE.md` «Quality enforcement»: линт-правила и `-W*`
  флаги нельзя отключать без явного согласования с пользователем.

Известные ограничения / не проверяется в CI:

- **STM32F401CE и STM32F411CE** формально поддерживаются кодом SDK, но
  больше не покрываются CI (у maintainer'а нет физического железа).
  Используйте на свой страх и риск; если соберёте под них и поймаете
  warning, ломающий `-Werror` — заведите issue.
- `bare-metal/blink/main.cpp` обновлён: `g_ticks = g_ticks + 1` вместо
  `++g_ticks` (C++20 deprecation `operator++` на volatile). Downstream-
  проектам, скопировавшим оригинальный код, нужна такая же однострочная
  правка.

Отложено в v0.1.5+:

- Все P1-команды `stmtool` из
  [issue #2](https://github.com/khosta77/stm32-sdk/issues/2):
  `monitor`, `size`, `config`, `project info`, `device list`, `chips`,
  `boards`.
- Новые bare-metal шаблоны: `uart-echo`, `spi-sensor`, `adc-dma`.
- Board definitions (`.toml` файлы) и Python chipdb.
- Флаг `-Wconversion` (много шума на CMSIS-коде; требует отдельной
  чистки).

### v0.1.3

Главные изменения:

- Новый сайт документации (MkDocs Material), английский и русский.
- Подкоманды `stmtool sdk update`, `sdk list-versions`, `sdk path`.
- Опциональная генерация `CLAUDE.md` per-template через `--with-claude`.
- Документированы все новые сенсоры (MPU6050, SSD1306, W25Q32) и DMA-обёртка.
- Исправлен I2C multi-byte read по RM0090 §27.3.3, корректен на 400 кГц.
- `GpioConfig` переписан как aggregate с валидатором `consteval gpio({...})`.

См. [заметки по апгрейду](migration.md) о необходимых правках в пользовательском коде.

### v0.1.2

- Усиление `stmtool` install (`install.sh` чистит кэш при переустановке).

### v0.1.1

- FreeRTOS-шаблоны (`freertos/blink`, `freertos/mpu6050-uart`) зарегистрированы
  для discovery в `stmtool`.

### v0.1.0

- Первый релиз. Версионирование через `setuptools-scm` для `stmtool` и SDK
  в целом. CI перенесён с `main` на `develop`.

## GitHub Pages

Сайт документации публикуется из ветки `gh-pages` (создаётся workflow
`docs.yml`). Адрес — <https://khosta77.github.io/stm32-sdk/>. После первого
деплоя — включить Pages в Settings → Pages → Source = `gh-pages` / `(root)`.
