# Configuration (Kconfig)

Since v0.2.2 the firmware content of a project is configured through
**Kconfig** — the same mechanism the Linux kernel and Zephyr use. The
project-local `.config` file is the single source of truth: subsystem gates,
logging, HSE frequency, float ABI and the FreeRTOS tunables all live there.
The old way (`set(STM32_USE_* ...)` in `CMakeLists.txt`, `-DSTM32_LOG_LEVEL=...`
on the command line) was **removed** — see the
[migration notes](migration.md#new-in-v022).

## Three files, three questions

| File | Question it answers | Owner |
|---|---|---|
| `stmproject.toml` | how to build and flash (SDK ref, chip, flash tool) | `stmtool`, host side |
| `.config` | what is inside the firmware | Kconfig; commit it to git |
| `CMakeLists.txt` | how to link (sources + libraries) | plain CMake, now thin |

Chip selection deliberately stays in `stmproject.toml` (the west model): the
Kconfig tree ships *with the SDK*, so the SDK ref and chip must be known
before the tree can even be parsed.

## Editing the configuration

```bash
stmtool config          # interactive menuconfig TUI (curses)
```

The TUI shows every option with its help text, enforces dependencies (e.g.
`STM32_USE_SENSORS` cannot be enabled without `STM32_USE_DRIVERS`) and writes
`.config` on save. A project without `.config` starts from the tree defaults —
the same command is therefore the migration path for pre-v0.2.2 projects.

`.config` is a plain text file and can be edited by hand; the build validates
it strictly. Any assignment the tree rejects — an unknown symbol, an
out-of-range value, a gate whose dependencies are not satisfied — fails the
configure step with a pointer to the offending line, it is never silently
dropped.

## What is configurable

Symbols appear in `.config` with the `CONFIG_` prefix.

### Subsystem gates

| Symbol | Enables | Depends on |
|---|---|---|
| `STM32_USE_DRIVERS` | driver modules + logging | — |
| `STM32_USE_FREERTOS` | FreeRTOS kernel + `rtos.hpp` wrappers | — |
| `STM32_USE_SENSORS` | MPU6050 / SSD1306 / W25Q32 | `STM32_USE_DRIVERS` |
| `STM32_USE_STORAGE` | flash storage layer | `STM32_USE_DRIVERS` |
| `STM32_USE_SYSTEM` | component framework + concurrency layer | `STM32_USE_DRIVERS` |
| `STM32_USE_TESTING` | on-device `ASSERT_*`/`EXPECT_*` helpers | — |

### Core

| Symbol | Default | Meaning |
|---|---|---|
| `STM32_HSE_VALUE` | `8000000` | external crystal frequency (Hz), 4–26 MHz |
| `STM32_FLOAT_ABI_HARD` / `_SOFTFP` / `_SOFT` | `hard` | float ABI choice; selects the FPU flags and the FreeRTOS port |

### Logging (with `STM32_USE_DRIVERS`)

| Symbol | Default | Meaning |
|---|---|---|
| `STM32_LOG_LEVEL_*` (choice `NONE`…`TRACE`) | `INFO` | compile-time ceiling; `LOG_*` above it vanish from flash |
| `STM32_LOG_BACKEND_SEL_*` (choice `NONE`/`ITM`/`UART`) | `NONE` | intended sink, exposed as the `STM32_LOG_BACKEND` define |

### FreeRTOS tunables (with `STM32_USE_FREERTOS`)

Previously hardcoded in the SDK's `FreeRTOSConfig.h`, configurable since
v0.2.2. Defaults reproduce the historic values exactly.

| Symbol | Default | Maps to |
|---|---|---|
| `FREERTOS_TOTAL_HEAP_SIZE` | `16384` | `configTOTAL_HEAP_SIZE` |
| `FREERTOS_TICK_RATE_HZ` | `1000` | `configTICK_RATE_HZ` |
| `FREERTOS_MAX_PRIORITIES` | `5` | `configMAX_PRIORITIES` |
| `FREERTOS_MINIMAL_STACK_SIZE` | `128` | `configMINIMAL_STACK_SIZE` (words) |
| `FREERTOS_TIMER_TASK_PRIORITY` | `2` | `configTIMER_TASK_PRIORITY` |
| `FREERTOS_TIMER_QUEUE_LENGTH` | `10` | `configTIMER_QUEUE_LENGTH` |
| `FREERTOS_TIMER_TASK_STACK_DEPTH` | `256` | `configTIMER_TASK_STACK_DEPTH` (words) |
| `FREERTOS_CHECK_FOR_STACK_OVERFLOW` | `2` | `configCHECK_FOR_STACK_OVERFLOW` |

The timer-task priority must stay strictly below `FREERTOS_MAX_PRIORITIES`;
the generator enforces this cross-check at configure time. Keep in mind that
SDK demos create tasks at priority 2 — lowering `FREERTOS_MAX_PRIORITIES`
below 3 would break them, and the range guard forbids it.

## How it works under the hood

```
.config ──┐
          ├─ sdk/scripts/kconfig/genconfig.py (kconfiglib, in the build image)
sdk/Kconfig ──┘        │
                       ├─ out/generated/config.cmake      → CMake variables
                       └─ out/generated/stm32_autoconf.h  → CONFIG_* macros
                                                            (FreeRTOSConfig.h reads them)
```

`sdk/cmake/kconfig.cmake` runs the generator on every configure, registers
`.config` and the Kconfig fragments as configure dependencies (editing them
re-triggers CMake), and also writes `out/generated/Kconfig.chip` — a
chip-derived fragment (`STM32_FAMILY_STM32F4`, chip name) that future
per-family options and the partition layer (#62) hang their `depends on` off.

Templates ship a `defconfig` with their gate set;
`stmtool project create` copies it into the new project as `.config`.

## Adding options for a new subsystem

Each subsystem owns a Kconfig fragment next to its sources
(`sdk/rtos/Kconfig`, `sdk/drivers/Kconfig`, ...), `source`d from the root
`sdk/Kconfig`. A new peripheral driver brings its options in its own
fragment — use `depends on STM32_USE_DRIVERS`, give every option a `help`
text and a safe default, and express invariants with `range`/`depends on`
rather than CMake checks. Cross-symbol constraints Kconfig cannot express
(strict inequalities) go into `_cross_checks` in
`sdk/scripts/kconfig/genconfig.py`.
