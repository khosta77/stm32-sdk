# Поддерживаемые чипы

SDK на данный момент целится в семейство STM32F4. Выбор чипа происходит на
этапе configure через `-DSTM32_CHIP=<name>` (автоматически устанавливается
`stmtool` из `stmproject.toml`).

| Семейство | RAM | CCM | Flash | Пример |
|-----------|-----|-----|-------|--------|
| STM32F401 | 64-96К | — | 128-512К | STM32F401CC, STM32F401RE |
| STM32F405 | 128К | 64К | 512-1024К | STM32F405RG |
| STM32F407 | 128К | 64К | 512-1024К | STM32F407VG |
| STM32F411 | 128К | — | 256-512К | STM32F411CE |
| STM32F412 | 256К | — | 512-1024К | STM32F412VG |
| STM32F429 | 192К | 64К | 512-2048К | STM32F429ZI |
| STM32F439 | 192К | 64К | 512-2048К | STM32F439ZI |
| STM32F446 | 128К | — | 256-512К | STM32F446RE |

Формат имени чипа — `STM32F4xxYZ`, где:

- `Y` — буква корпуса (R/V/Z/…).
- `Z` — буква размера flash:
    - `B` → 128 КиБ
    - `C` → 256 КиБ
    - `E` → 512 КиБ
    - `G` → 1 МиБ
    - `I` → 2 МиБ

Размер flash декодируется автоматически в `sdk/cmake/families/stm32f4.cmake`.

## Индекс чипов

`sdk/chips.json` — единственный источник правды о том, какие семейства
поддерживает SDK. `stm32_families.cmake` читает его, чтобы понять, известно ли
семейство чипа, поддерживается ли оно и есть ли для него family-файл; stmtool
читает его для автодополнения и чтобы отвергнуть неподдерживаемый чип до начала
сборки.

До v0.2.4 то же самое хранилось в шестнадцати байт-в-байт одинаковых заглушках
«not yet supported» плюс в захардкоженном списке внутри stmtool, так что
добавление семейства означало правку в семнадцати местах, и любое из них могло
устареть.

Неподдерживаемый чип теперь сообщает список поддерживаемых семейств из этого
одного файла:

```
The G0 family is not yet supported by stm32-sdk.
Chip requested: STM32G0B0RE
Currently supported: stm32f4
```

## Добавление нового семейства

1. Переключить `supported` в `true` для семейства в `sdk/chips.json` и
   перечислить конкретные партномера в `chips`.
2. Создать `sdk/cmake/families/stm32XX.cmake` с функцией `stm32XX_get_chip_info`,
   заполняющей RAM, CCM, flash, флаги CPU.
3. Создать `sdk/drivers/families/stm32XX.cmake`, перечислив модули
   `driver/stm32XX/*.cppm` этого семейства в `STM32_DRIVER_FAMILY_MODULES` в
   порядке импорта.
4. Положить CMSIS device headers в `sdk/hal/stm32XX/include/cmsis/`, а также
   `sdk/hal/stm32XX/include/cmsis_device.h` — семейно-нейтральную обёртку,
   которую включает общий код (`system.work_queue`, ITM-бэкенд лога) вместо
   имени конкретного device-заголовка.
5. Добавить vector tables в `sdk/hal/stm32XX/src/cmsis/`.
6. Создать `sdk/hal/stm32XX/ldscripts/mem.ld.in`.
7. Для семейства на Cortex-M0/M0+ добавить его в `_stm32_cortex_m0_families` в
   `sdk/cmake/kconfig.cmake`, чтобы `STM32_CORE_HAS_ITM` получился `n` и
   ITM-бэкенд лога скрывался, а не падал при компиляции.

Конкретный пример реализации — [STM32F4](stm32f4.md).

## Даташиты

См. [Даташиты](datasheets.md) — прямые ссылки на reference manuals, datasheets
и programming manuals от ST.
