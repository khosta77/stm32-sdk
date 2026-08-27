# Флаги компилятора и предупреждения

Все compile-флаги SDK задаются в одной точке — на INTERFACE-таргете
`stm32_core` в файле `sdk/cmake/stm32_sdk.cmake`. Любая другая
библиотека (`stm32_hal`, `stm32_drivers`, `stm32_rtos`, `stm32_sensors`,
`stm32_storage`, `stm32_system`, `stm32_testing`) — а также любой
пользовательский проект, который делает
`target_link_libraries(... stm32_core)`, — наследует тот же набор. Отдельной
политики «только для SDK» против «только для приложения» нет.

Header-only хелперы юнит-тестов подключаются опционально через
`STM32_USE_TESTING`, добавляющий INTERFACE-таргет `stm32_testing` (только путь
`sdk/testing/include` — без исходников и зависимостей линковки). См.
[Testing](modules/testing.md).

Внутри `stm32_system` модули компилируются условно: `system.component`,
`system.bootstrap` и `system.work_queue` строятся всегда при включённом
`STM32_USE_SYSTEM`, а `system.executor` и `system.signal_bus` (которым нужны
обёртки FreeRTOS) — только когда дополнительно включён `STM32_USE_FREERTOS`;
ядро очереди остаётся пригодным для bare-metal-сборок.

## Конфигурация — это Kconfig (v0.2.2)

Гейты подсистем (`STM32_USE_*`), уровень и backend
[логирования](modules/logging.md), частота HSE, float ABI и тюнаблы FreeRTOS —
всё это настраивается через проектный файл `.config`, см.
[Конфигурацию](configuration.ru.md). Передача их как `-D` cache-переменных
удалена в v0.2.2; значения ниже приходят из генерируемого
`out/generated/config.cmake`.

Choice уровня логирования по-прежнему отображается в compile-define на цели
драйверов: `LOG_*` выше потолка раскрываются в `((void)0)` (строки-форматы не
попадают во flash), а предполагаемый sink выставляется как define
`STM32_LOG_BACKEND` (со значениями `STM32_LOG_BACKEND_NONE`/`_ITM`/`_UART`),
чтобы код приложения выбрал подходящий тип backend'а обобщённо.

## Окружение сборки (Docker, офлайн)

Начиная с v0.1.13 любая сборка идёт внутри Docker-образа SDK
(`ghcr.io/khosta77/stm32-sdk-build:latest`) — у `stmtool build` нет пути через
хостовый тулчейн. Образ пинит `arm-none-eabi-gcc` 15.2 (GCC >= 14 обязателен для
сканирования зависимостей модулей C++20; иначе `stm32_sdk.cmake` прерывает
конфигурацию) и хостовый `g++` для [host-тестов](modules/testing.md).

Образ также **вшивает FreeRTOS-Kernel** (`V11.1.0`) в `/opt/freertos-kernel` и
экспортирует `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL`, поэтому сборки с FreeRTOS
больше не клонируют ядро для каждого проекта и работают офлайн. Вне Docker
`stm32_rtos.cmake` по-прежнему уважает эту переменную (в том числе из окружения)
для заранее подготовленного чекаута, откатываясь к shallow-клону `FetchContent`.

## Активные флаги (v0.1.4)

```cmake
target_compile_options(stm32_core INTERFACE
    ${STM32_ARCH_FLAGS}      # -mcpu, -mfpu, -mfloat-abi от семейства чипа
    -Os                      # по умолчанию размер; -O0 через CMAKE_BUILD_TYPE
    -ffreestanding
    -ffunction-sections
    -fdata-sections
    -fsigned-char
    -fno-move-loop-invariants
    -Wall                    # стандартный набор
    -Wextra
    -Wpedantic               # соответствие ISO
    -Wshadow                 # ловит скрытие имени из внешней области
    -Werror                  # предупреждения = ошибки — см. политику ниже
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>
)
```

`-Wconversion` сознательно не включён — на рукописном embedded-коде, где
смешиваются 8/16/32-битные регистровые поля, он выдаёт стену шума без
реальных находок. Возможно вернётся в одном из будущих релизов после
отдельного прохода чистки.

## Исключения и RTTI выключены (v0.2.4)

С v0.2.4 каждая C++-цель несёт:

```
-fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables
```

В рантайме SDK никогда ничего не бросал — валидаторы конфигов сообщают о
неверном вводе через `static_assert`, на этапе компиляции, — поэтому машинерия
исключений была мёртвым весом. Её удаление выбрасывает таблицы
`.ARM.exidx` / `.ARM.extab` *и вместе с ними* раскрутчик стека и
personality-функцию, которые притягивает libstdc++:

| шаблон | flash до | после |
|---|---|---|
| `bare-metal/blink` | 5892 | 1724 (−71%) |
| `freertos/signal-bus-demo` | 21796 | 14236 (−35%) |
| `freertos/w25q32-flash-test` | 23796 | 16344 (−31%) |
| `freertos/imu-flash-oled-demo` | 25812 | 18016 (−30%) |

Следствие для прикладного кода: `try` / `catch` / `throw` / `dynamic_cast` /
`typeid` не компилируются.

Флаги живут в одной переменной `STM32_CXX_DIALECT_FLAGS` в
`sdk/cmake/stm32_sdk.cmake` и применяются к `stm32_core` *и* к каждой
OBJECT/STATIC-библиотеке, которые намеренно не линкуют `stm32_core`. Это не
вопрос аккуратности: GCC хранит диалект в BMI каждого модуля, поэтому
рассогласование producer и consumer прямо ломает импорт:

```
error: module driver.gpio: language dialect differs 'C++20',
       expected 'C++20/no-exceptions'
```

`-fno-exceptions` и `-fno-rtti` завёрнуты в `$<$<COMPILE_LANGUAGE:CXX>:...>`,
потому что `stm32_rtos` компилирует ещё и C, где GCC выдал бы на них
предупреждение, а `-Werror` превратил бы его в ошибку.

## Политика `-Werror`

`-Werror` включён постоянно начиная с v0.1.4. Любое предупреждение,
дошедшее до компилятора — это ошибка сборки. Это касается и самого SDK,
и проектов, созданных через `stmtool project create`.

**Не отключайте `-Werror`**, чтобы сборка стала зелёной. Вместо этого:

1. **Поправьте исходный код**, если предупреждение в коде, которым вы
   управляете. Большинство `-Wshadow` и `-Wpedantic` лечится одной
   строкой.
2. **Подавите на уровне файла**, если предупреждение в vendor-коде
   (CMSIS device headers, сгенерированные таблицы векторов, исходники
   FreeRTOS). Используйте `set_source_files_properties` рядом с этим
   конкретным файлом, не глобальный `-Wno-*`. Пример из SDK:

   ```cmake
   set_source_files_properties(
       ${STM32_HAL_DIR}/src/cmsis/${STM32_VECTORS_FILE}
       PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
   )
   ```

   Так предупреждение остаётся видимым везде, кроме одного
   поимённого файла.

В downstream-проектах правило то же: если ваше приложение использует
vendor-SDK, который триггерит предупреждения — подавляйте их локально
на этих исходниках, а не на всём таргете.

## Принуждение `[[nodiscard]]` (v0.1.5)

`driver::Status` и `driver::Result<T>` помечены `[[nodiscard]]`. Вместе с
`-Werror` это превращает любой проигнорированный возврат ошибки в ошибку
сборки — компилятор гарантирует, что каждый `Status` проверен. Когда сброс
действительно намеренный (например, потеря байта в полном ISR-кольце), его
делают явным через `(void)`:

```cpp
(void) _rxBuf.push(byte);  // ISR: сброс байта при полном RX-буфере
```

Это осознанная политика на этапе компиляции, а не предупреждение, которое
надо подавлять. См. [драйверы](modules/drivers.ru.md) про `Result<T>` и
хелперы `DRV_TRY`.

## Исключения для C++

Файл `sdk/core/src/newlib/cxx.cpp` — это translation unit для C++
runtime support. Он обязан компилироваться с C++ образца GCC 11, чтобы
сохранялся ABI-контракт с newlib. SDK задаёт это per-file override'ом:

```cmake
set_source_files_properties(
    ${_STM32_SDK_DIR}/core/src/newlib/cxx.cpp
    PROPERTIES COMPILE_OPTIONS
        "-std=gnu++11;-fabi-version=0;-fno-exceptions;-fno-rtti;-fno-use-cxa-atexit;-fno-threadsafe-statics"
)
```

Всё остальное, включая `main.cpp` шаблонов, компилируется в C++20
(`gnu++20`).
