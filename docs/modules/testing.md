# Testing — on-device unit tests

Since v0.1.11 the SDK ships a minimal, GTest-style unit-test framework as a
single header, `testing/unit_test.hpp`, under `sdk/testing/include/`. It gives
you `ASSERT_*` / `EXPECT_*` assertions and a tiny runner that reports pass/fail
over any character sink you provide.

It is built for bare metal: **no exceptions, no RTTI, no heap, no dynamic
linking**, and **no `<cstdio>` / `snprintf` dependency** (integers are formatted
by hand). Because the output sink is injected, the exact same test bodies
compile and run both on the host (sink → `stdout`) and on the device (sink →
UART / SWO). It complements the host-test mock buses and CI (issues #34, #35):
those give a host runner, this gives the assertion vocabulary shared by both.

Enable it with `STM32_USE_TESTING`, link the header-only `stm32_testing` target.
See [build flags](../build-flags.md).

## Writing tests

A test is a plain function declared with `TEST(name)`; the `ASSERT_*` /
`EXPECT_*` macros act on an implicit per-test context `_t`.

```cpp
#include "testing/unit_test.hpp"
import driver.types;
using driver::Result;
using driver::Status;

TEST(result_ok_carries_value) {
  Result<int> r{42};
  ASSERT_TRUE(r.ok());       // stops this test on failure
  EXPECT_EQ(r.value(), 42);  // records failure, keeps going
  EXPECT_EQ(r.valueOr(0), 42);
}

TEST(result_error_reports_status) {
  Result<int> r{Status::Timeout};
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status(), Status::Timeout);
  EXPECT_EQ(r.valueOr(7), 7);
}
```

- `EXPECT_*` records a failure and continues the test.
- `ASSERT_*` records a failure and `return`s from the test immediately — the
  rest of that test body does not run. On a platform without exceptions this is
  exactly how GTest's fatal assertions behave; here the test *is* a function, so
  a plain `return` suffices.

Available checks: `ASSERT_TRUE` / `ASSERT_FALSE` / `ASSERT_EQ` / `ASSERT_NE`
and the `EXPECT_` counterparts. They are type-agnostic — they work with
integers, pointers, `bool`, and enums such as `driver::Status`.

## Running tests

There is **no auto-registration** — the caller drives an explicit run list. This
keeps ordering deterministic and avoids static constructors running before
`main()` on bare metal.

```cpp
void uartWrite(const char *s);  // your sink: pushes bytes to USART

int main() {
  testing::TestRunner runner{uartWrite};

  RUN_TEST(runner, result_ok_carries_value);
  RUN_TEST(runner, result_error_reports_status);

  const bool ok = runner.summary();  // "N passed, M failed"
  // ok == true when every test passed
  while (true) {
  }
}
```

- `testing::TestRunner runner{writer}` — `writer` is a
  `testing::Writer` = `void (*)(const char *)`.
- `RUN_TEST(runner, name)` runs one test and tallies the result, printing
  `[PASS] name` or `[FAIL] name` (with a `  fail: <expression>` line per failed
  check).
- `runner.summary()` prints `N passed, M failed` and returns `true` when
  `failed() == 0`. `passed()` / `failed()` expose the counts.

## Host + device from one source

The sink is the only platform-specific part. On the device it is a UART writer;
on the host, point it at `stdout` and the same test file builds with a stock
compiler:

```cpp
#include <cstdio>
#include "testing/unit_test.hpp"

static void stdoutWriter(const char *s) {
  std::fputs(s, stdout);
}

int main() {
  testing::TestRunner runner{stdoutWriter};
  RUN_TEST(runner, result_ok_carries_value);
  return runner.summary() ? 0 : 1;  // non-zero exit on any failure
}
```

The `bare-metal/unit-test-demo` template is the worked device example; its
tests exercise `driver::Result<T>`, which is pure logic, so they are meaningful
on either target.

## Mock buses (v0.1.13)

Because the bus interfaces are C++20 **concepts**, a mock is just a plain struct
that satisfies the concept. The SDK ships reusable, programmable mocks as the
module `testing.mock` (`sdk/testing/mock/mock_bus.cppm`):

| Mock | Models | Programmable via |
|------|--------|------------------|
| `testing::MockI2c` | `driver::II2c` | `loadResponse(...)`, `written()`, `nextStatus` |
| `testing::MockSpi` | `driver::ISpi` | `loadResponse(...)`, `written()`, `nextStatus` |
| `testing::MockUart` | `driver::IUart` | `loadRx(...)`, `transmitted()` |
| `testing::MockGpioPin` | `driver::IGpioPin` | `set/reset/toggle` counters, level |
| `testing::MockFlash` | `driver::IFlash` | in-memory array, `eraseSector` |

`loadResponse` / `loadRx` script the bytes a read returns; `written()` /
`transmitted()` expose what the code under test sent, so you assert behaviour
rather than only a return status. `nextStatus` injects a bus error. They are
heap-free and exception-free, so they also compile on device.

```cpp
import sensor.mpu6050;
import testing.mock;

TEST(mpu6050_scales_accel) {
  testing::MockI2c bus;
  const uint8_t frame[14] = {0x40, 0x00};  // ACCEL_XOUT hi/lo -> 1 g
  bus.loadResponse({frame, 14});

  sensor::Mpu6050 mpu{bus, kCfg};
  sensor::ImuData data;
  ASSERT_EQ(mpu.read(data), Status::Ok);
  EXPECT_EQ(bus.lastReg, 0x3B);  // addressed ACCEL_XOUT_H
  EXPECT_TRUE(data.accel.x > 9.8f && data.accel.x < 9.82f);
}
```

The module is not linked into the `stm32_testing` INTERFACE target (it imports
the driver concept modules); a consumer opts in by adding `mock_bus.cppm` to its
own `CXX_MODULES` file set alongside `stm32_drivers`.

## Host tests and `stmtool test` (v0.1.13)

`tests/host/` is a standalone CMake tree, built with the image's host `g++`,
that compiles the CMSIS-free portable modules plus `testing.mock` into real
executables and runs them under `ctest`. Run the whole suite with:

```bash
stmtool test
```

This builds and runs the tests inside the SDK Docker image (into `out/host`) and
exits non-zero if anything fails; CI runs the same command in its `host-tests`
job. To add a case, drop a `test_*.cpp` into `tests/host/` and register it in
`tests/host/CMakeLists.txt` via `add_host_test(...)`.

Two rules keep C++20 modules happy in a test translation unit that both imports
modules and includes textual headers:

- **Include STL / textual headers before the `import` lines.** A module includes
  e.g. `<cstddef>` in its global module fragment; including it textually *after*
  importing that module triggers `redefinition of std::__byte_operand`.
- **Do not `#include <span>`; brace-initialise spans** (`bus.loadResponse({p, n})`)
  and use `auto` for span returns — mixing `#include <span>` with an `import` of a
  module that includes `<span>` is rejected by GCC 15.
