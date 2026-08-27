# Logging

A compile-time-filtered logging facility for bare-metal targets (since
v0.1.14). It ships with the driver library (`STM32_USE_DRIVERS`) and follows the
same zero-vtable / zero-heap style as the rest of the SDK: the output backend is
a captureless function pointer, never `std::function`, and there is no dynamic
allocation.

## Two levels of filtering

The facility has two independent filters, which is what makes it cheap in a
release build and flexible during bring-up:

- **Compile-time (`STM32_LOG_LEVEL`).** The `LOG_*` macros above the configured
  ceiling expand to `((void)0)`. Both the call *and its format string* are
  removed from the binary, so silenced levels cost zero flash and zero cycles.
- **Runtime (`driver::log::setLevel`).** Of the calls that survived compilation,
  records strictly above the runtime level are dropped without a rebuild — handy
  for turning `Debug` on over a serial command.

A record is emitted only when its level is `<=` **both** ceilings.

## Levels

| Level          | Value | Macro prefix |
|----------------|-------|--------------|
| `None`         | 0     | (silences everything) |
| `Error`        | 1     | `LOG_ERROR`  |
| `Warn`         | 2     | `LOG_WARN`   |
| `Info`         | 3     | `LOG_INFO`   |
| `Debug`        | 4     | `LOG_DEBUG`  |
| `Trace`        | 5     | `LOG_TRACE`  |

## Using it

`import driver.log;`, then include the macro header **after** the import (macros
cannot be exported by a C++20 module, so they live in a textual header — the same
pattern as `driver/try.hpp`):

```cpp
import driver.log;
import driver.log.uart;      // the backend you picked
import driver.uart;          // its bus, for the UART backend
import driver.stm32f4.uart;

#include "driver/log.hpp"    // LOG_* macros, AFTER the imports
```

Install a backend once at startup and set the runtime level, then log:

```cpp
Uart<> g_uart{*USART2, USART2_IRQn, uart<{...}>()};
driver::log::UartBackend g_logSink{g_uart};

int main() {
  g_logSink.install();
  driver::log::setLevel(driver::log::Level::Debug);

  LOG_INFO("boot", "system up");
  LOG_DEBUG_U32("adc", "raw=", reading);      // "D [adc] raw=1234"
  LOG_ERROR_HEX("reg", "CR1=", USART2->CR1);  // "E [reg] CR1=0x2000000c"
}
```

Each level has three macro forms:

- `LOG_INFO(tag, msg)` — a bare message.
- `LOG_INFO_U32(tag, msg, value)` — appends a `uint32_t` in decimal.
- `LOG_INFO_HEX(tag, msg, value)` — appends a `uint32_t` as `0x`-prefixed
  lowercase hex.

Output is framed as `<L> [tag] msg\r\n`, where `<L>` is the level's initial
(`E`/`W`/`I`/`D`/`T`). Formatting is done by hand (no `printf`/`snprintf`), so it
works under `nano.specs` and adds no float-formatting weight.

## Selecting the compile-time level

Since v0.2.2 the level is a Kconfig choice in the project `.config`
(`STM32_LOG_LEVEL_NONE` … `STM32_LOG_LEVEL_TRACE`, default `INFO`) — edit it
with `stmtool config`, see [Configuration](../configuration.md). The old
`-DSTM32_LOG_LEVEL=...` cache variable was removed.

The choice still maps to the `STM32_LOG_LEVEL=<0..5>` compile definition on
the driver target; the header defaults to `INFO` when the define is absent. A
release build that selects `NONE` (or any level below your
`LOG_DEBUG`/`LOG_TRACE` calls) strips those statements entirely.

## Backends

A backend is a sink — a `void(*)(void* ctx, const char* data, size_t len)` thunk
installed via `driver::log::setSink`. The SDK ships two, both exposing
`install()`:

- **`driver::log::ItmBackend`** (`import driver.log.itm`) — streams every byte to
  ITM stimulus port 0, read back over SWO by the debugger (OpenOCD `itm port 0
  on`, ST-Link SWV). Stateless. If the debugger has not enabled trace,
  `ITM_SendChar` is a no-op, so it is safe to install unconditionally.
- **`driver::log::UartBackend<UartDriver>`** (`import driver.log.uart`) — forwards
  to any `driver::IUart` implementation, so logs share the application's serial
  line. Generic over the bus in the terse constrained style; CTAD deduces the
  parameter from the constructor (`UartBackend g_sink{g_uart}`).

The intended backend is named in `.config` via the Kconfig choice
`STM32_LOG_BACKEND_SEL_NONE` / `_ITM` / `_UART` (default `NONE`) — same
place as the level, see [Configuration](../configuration.md).

Both backend modules always compile (a template and inline CMSIS code cost
nothing until instantiated), so the choice does not gate availability — it sets the
`STM32_LOG_BACKEND` define (with the symbolic `STM32_LOG_BACKEND_NONE` / `_ITM` /
`_UART` values), letting application code pick the matching type generically:

```cpp
#if STM32_LOG_BACKEND == STM32_LOG_BACKEND_ITM
driver::log::ItmBackend g_logSink;
#else
driver::log::UartBackend g_logSink{g_uart};
#endif
```

## Custom backend

Any function with the sink signature works. Install it directly:

```cpp
void mySink(void* /*ctx*/, const char* data, size_t len) {
  for (size_t i = 0; i < len; ++i) semihostPut(data[i]);
}

driver::log::setSink(&mySink, nullptr);
```

`ctx` is passed back verbatim, so a stateful backend can carry `this` through it
— exactly how `UartBackend` reaches its captured `UartDriver&`.

## Host testing

`driver.log` is CMSIS-free, so it compiles and runs on the host. `tests/host/`
exercises the framing, the runtime gate, and decimal/hex formatting against a
capturing sink — see [Testing](testing.md).
