# Флаги компилятора и предупреждения

Все compile-флаги SDK задаются в одной точке — на INTERFACE-таргете
`stm32_core` в файле `sdk/cmake/stm32_sdk.cmake`. Любая другая
библиотека (`stm32_hal`, `stm32_drivers`, `stm32_rtos`, `stm32_sensors`,
`stm32_storage`) — а также любой пользовательский проект, который делает
`target_link_libraries(... stm32_core)`, — наследует тот же набор. Отдельной
политики «только для SDK» против «только для приложения» нет.

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
реальных находок. Возможно вернётся в v0.1.5 после отдельного прохода
чистки.

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
