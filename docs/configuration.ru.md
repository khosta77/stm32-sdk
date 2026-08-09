# Конфигурация (Kconfig)

Начиная с v0.2.2 содержимое прошивки проекта настраивается через
**Kconfig** — тот же механизм, что используют ядро Linux и Zephyr.
Проектный файл `.config` — единственный источник правды: гейты подсистем,
логирование, частота HSE, float ABI и тюнаблы FreeRTOS живут в нём.
Старый способ (`set(STM32_USE_* ...)` в `CMakeLists.txt`,
`-DSTM32_LOG_LEVEL=...` в командной строке) **удалён** — см.
[заметки по апгрейду](migration.ru.md#v022).

## Три файла — три вопроса

| Файл | На какой вопрос отвечает | Владелец |
|---|---|---|
| `stmproject.toml` | как собирать и прошивать (реф SDK, чип, инструмент прошивки) | `stmtool`, хост-сторона |
| `.config` | что находится внутри прошивки | Kconfig; коммитьте в git |
| `CMakeLists.txt` | как линковать (исходники + библиотеки) | обычный CMake, теперь тонкий |

Выбор чипа намеренно остаётся в `stmproject.toml` (модель west): дерево
Kconfig поставляется *вместе с SDK*, поэтому реф SDK и чип должны быть
известны ещё до того, как дерево вообще можно распарсить.

## Редактирование конфигурации

```bash
stmtool config          # интерактивный menuconfig-TUI (curses)
```

TUI показывает каждую опцию с её help-текстом, следит за зависимостями
(например, `STM32_USE_SENSORS` нельзя включить без `STM32_USE_DRIVERS`) и
записывает `.config` при сохранении. Проект без `.config` стартует с
дефолтов дерева — поэтому та же команда служит путём миграции для проектов
до v0.2.2.

`.config` — обычный текстовый файл, его можно править руками; сборка
валидирует его строго. Любое присваивание, которое дерево отвергает —
неизвестный символ, значение вне диапазона, гейт с неудовлетворёнными
зависимостями, — роняет шаг конфигурации с указанием на проблемную строку
и никогда не отбрасывается молча.

## Что настраивается

Символы появляются в `.config` с префиксом `CONFIG_`.

### Гейты подсистем

| Символ | Включает | Зависит от |
|---|---|---|
| `STM32_USE_DRIVERS` | модули драйверов + логирование | — |
| `STM32_USE_FREERTOS` | ядро FreeRTOS + RAII-обёртки `rtos.hpp` | — |
| `STM32_USE_SENSORS` | MPU6050 / SSD1306 / W25Q32 | `STM32_USE_DRIVERS` |
| `STM32_USE_STORAGE` | слой флэш-партиций | `STM32_USE_DRIVERS` |
| `STM32_USE_SYSTEM` | каркас компонентов + слой конкурентности | `STM32_USE_DRIVERS` |
| `STM32_USE_TESTING` | хелперы `ASSERT_*`/`EXPECT_*` на устройстве | — |

### Ядро

| Символ | По умолчанию | Значение |
|---|---|---|
| `STM32_HSE_VALUE` | `8000000` | частота внешнего кварца (Гц), 4–26 МГц |
| `STM32_FLOAT_ABI_HARD` / `_SOFTFP` / `_SOFT` | `hard` | choice float ABI; определяет флаги FPU и порт FreeRTOS |

### Логирование (при `STM32_USE_DRIVERS`)

| Символ | По умолчанию | Значение |
|---|---|---|
| `STM32_LOG_LEVEL_*` (choice `NONE`…`TRACE`) | `INFO` | compile-time потолок; `LOG_*` выше него исчезают из flash |
| `STM32_LOG_BACKEND_SEL_*` (choice `NONE`/`ITM`/`UART`) | `NONE` | предполагаемый sink, выставляется как define `STM32_LOG_BACKEND` |

### Тюнаблы FreeRTOS (при `STM32_USE_FREERTOS`)

Раньше были захардкожены в `FreeRTOSConfig.h` SDK, с v0.2.2 настраиваются.
Дефолты в точности воспроизводят исторические значения.

| Символ | По умолчанию | Отображается в |
|---|---|---|
| `FREERTOS_TOTAL_HEAP_SIZE` | `16384` | `configTOTAL_HEAP_SIZE` |
| `FREERTOS_TICK_RATE_HZ` | `1000` | `configTICK_RATE_HZ` |
| `FREERTOS_MAX_PRIORITIES` | `5` | `configMAX_PRIORITIES` |
| `FREERTOS_MINIMAL_STACK_SIZE` | `128` | `configMINIMAL_STACK_SIZE` (слова) |
| `FREERTOS_TIMER_TASK_PRIORITY` | `2` | `configTIMER_TASK_PRIORITY` |
| `FREERTOS_TIMER_QUEUE_LENGTH` | `10` | `configTIMER_QUEUE_LENGTH` |
| `FREERTOS_TIMER_TASK_STACK_DEPTH` | `256` | `configTIMER_TASK_STACK_DEPTH` (слова) |
| `FREERTOS_CHECK_FOR_STACK_OVERFLOW` | `2` | `configCHECK_FOR_STACK_OVERFLOW` |

Приоритет таймерной задачи обязан быть строго меньше
`FREERTOS_MAX_PRIORITIES`; генератор проверяет это кросс-условие на этапе
конфигурации. Имейте в виду: демо SDK создают задачи с приоритетом 2 —
снижение `FREERTOS_MAX_PRIORITIES` ниже 3 их сломало бы, и защита диапазона
это запрещает.

## Как это работает под капотом

```
.config ──┐
          ├─ sdk/scripts/kconfig/genconfig.py (kconfiglib, в build-образе)
sdk/Kconfig ──┘        │
                       ├─ out/generated/config.cmake      → переменные CMake
                       └─ out/generated/stm32_autoconf.h  → макросы CONFIG_*
                                                            (их читает FreeRTOSConfig.h)
```

`sdk/cmake/kconfig.cmake` запускает генератор на каждой конфигурации,
регистрирует `.config` и фрагменты Kconfig как configure-зависимости (их
правка перезапускает CMake), а также пишет `out/generated/Kconfig.chip` —
производный от чипа фрагмент (`STM32_FAMILY_STM32F4`, имя чипа), на который
будущие per-family опции и слой партиций (#62) вешают свои `depends on`.

Шаблоны поставляют `defconfig` со своим набором гейтов;
`stmtool project create` копирует его в новый проект как `.config`.

## Добавление опций для новой подсистемы

Каждая подсистема владеет фрагментом Kconfig рядом со своими исходниками
(`sdk/rtos/Kconfig`, `sdk/drivers/Kconfig`, ...), подключаемым через
`source` из корневого `sdk/Kconfig`. Новый драйвер периферии приносит свои
опции в собственном фрагменте — используйте `depends on STM32_USE_DRIVERS`,
давайте каждой опции `help`-текст и безопасный дефолт, а инварианты
выражайте через `range`/`depends on`, а не через проверки в CMake.
Кросс-символьные ограничения, которые Kconfig выразить не может (строгие
неравенства), идут в `_cross_checks` в
`sdk/scripts/kconfig/genconfig.py`.
