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
