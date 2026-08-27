# Chips overview

The SDK currently targets the STM32F4 family. Chip selection happens at
configure time via `-DSTM32_CHIP=<name>` (set automatically by `stmtool` from
`stmproject.toml`).

| Family | RAM | CCM | Flash | Example part |
|--------|-----|-----|-------|--------------|
| STM32F401 | 64-96K | — | 128-512K | STM32F401CC, STM32F401RE |
| STM32F405 | 128K | 64K | 512-1024K | STM32F405RG |
| STM32F407 | 128K | 64K | 512-1024K | STM32F407VG |
| STM32F411 | 128K | — | 256-512K | STM32F411CE |
| STM32F412 | 256K | — | 512-1024K | STM32F412VG |
| STM32F429 | 192K | 64K | 512-2048K | STM32F429ZI |
| STM32F439 | 192K | 64K | 512-2048K | STM32F439ZI |
| STM32F446 | 128K | — | 256-512K | STM32F446RE |

The chip-name convention is `STM32F4xxYZ` where:

- `Y` — package letter (R/V/Z/…).
- `Z` — flash size letter:
    - `B` → 128 KiB
    - `C` → 256 KiB
    - `E` → 512 KiB
    - `G` → 1 MiB
    - `I` → 2 MiB

Flash size is decoded automatically by `sdk/cmake/families/stm32f4.cmake`.

## The chip index

`sdk/chips.json` is the single source of truth for which families the SDK
supports. `stm32_families.cmake` reads it to decide whether a chip's family is
known, supported, and has a family file; stmtool reads it for shell completion
and to reject an unsupported chip before a build starts.

Before v0.2.4 the same knowledge lived in sixteen byte-identical "not yet
supported" family stubs plus a hardcoded list inside stmtool, so adding a
family meant editing seventeen places and any one of them could go stale.

An unsupported chip now reports the supported list out of that one file:

```
The G0 family is not yet supported by stm32-sdk.
Chip requested: STM32G0B0RE
Currently supported: stm32f4
```

## Adding a new family

1. Flip `supported` to `true` for the family in `sdk/chips.json` and list its
   concrete part numbers under `chips`.
2. Create `sdk/cmake/families/stm32XX.cmake` with a `stm32XX_get_chip_info`
   function that fills RAM, CCM, flash size, and CPU flags.
3. Create `sdk/drivers/families/stm32XX.cmake` listing that family's
   `driver/stm32XX/*.cppm` modules in `STM32_DRIVER_FAMILY_MODULES`, in
   import order.
4. Add CMSIS device headers under `sdk/hal/stm32XX/include/cmsis/`, plus
   `sdk/hal/stm32XX/include/cmsis_device.h` — the family-neutral wrapper that
   common code (`system.work_queue`, the ITM log backend) includes instead of
   naming a device header.
5. Add vector tables under `sdk/hal/stm32XX/src/cmsis/`.
6. Create `sdk/hal/stm32XX/ldscripts/mem.ld.in`.
7. On a Cortex-M0/M0+ family, add it to `_stm32_cortex_m0_families` in
   `sdk/cmake/kconfig.cmake` so `STM32_CORE_HAS_ITM` comes out `n` and the ITM
   log backend is hidden rather than failing to compile.

See [STM32F4](stm32f4.md) for a concrete implementation reference.

## Datasheets

See [Datasheets](datasheets.md) for direct links to ST's reference manuals,
datasheets, and programming manuals.
