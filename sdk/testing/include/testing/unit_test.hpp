#ifndef STM32_SDK_TESTING_UNIT_TEST_HPP
#define STM32_SDK_TESTING_UNIT_TEST_HPP

#include <cstddef>
#include <cstdint>

// Minimal GTest-style unit-test helpers for bare-metal targets.
//
// Design constraints (bare-metal / freestanding):
//   * No exceptions, no RTTI, no heap, no dynamic linking. ASSERT_* aborts the
//     current test by `return`ing from it (GTest can't do that without a
//     fixture; here the test IS a function, so a plain return works).
//   * No <cstdio> / snprintf dependency. All output goes through an injected
//     Writer (void(*)(const char*)); integers are formatted by hand. That keeps
//     the exact same test code compilable on the host (Writer -> stdout) and on
//     the device (Writer -> UART/SWO), so a test file can run in both places.
//
// Complements the host-test mock buses / CI (issues #34, #35): those give a
// host runner, this gives the assertion vocabulary shared by host and device.

namespace testing {

using Writer = void (*)(const char *);

namespace detail {

inline void writeUint(Writer write, uint32_t value) {
    char buf[11];
    char *p = buf + sizeof(buf);
    *--p = '\0';
    do {
        *--p = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0);
    write(p);
}

}  // namespace detail

// Per-test state. `failed` latches on the first failed check; `checks` counts
// every EXPECT_/ASSERT_ evaluated. A test reads/writes it only through macros.
struct TestContext {
    Writer write;
    const char *name;
    bool failed;
    uint32_t checks;
};

inline void reportFailure(const TestContext &ctx, const char *expr) {
    if (ctx.write != nullptr) {
        ctx.write("  fail: ");
        ctx.write(expr);
        ctx.write("\r\n");
    }
}

// Runs tests one by one and tallies pass/fail. No registration list -- the
// caller drives an explicit run list via RUN_TEST, which keeps ordering
// deterministic and avoids static constructors before main() on bare metal.
class TestRunner {
public:
    explicit TestRunner(Writer write) : _write(write) {}

    void run(const char *name, void (*fn)(TestContext &)) {
        TestContext ctx{_write, name, false, 0};
        fn(ctx);
        emit(ctx.failed ? "[FAIL] " : "[PASS] ", name);
        if (ctx.failed) {
            ++_failed;
        } else {
            ++_passed;
        }
    }

    // Prints "N passed, M failed" and returns true when every test passed.
    [[nodiscard]] bool summary() const {
        if (_write != nullptr) {
            detail::writeUint(_write, _passed);
            _write(" passed, ");
            detail::writeUint(_write, _failed);
            _write(" failed\r\n");
        }
        return _failed == 0;
    }

    [[nodiscard]] uint32_t passed() const { return _passed; }
    [[nodiscard]] uint32_t failed() const { return _failed; }

private:
    void emit(const char *prefix, const char *name) const {
        if (_write != nullptr) {
            _write(prefix);
            _write(name);
            _write("\r\n");
        }
    }

    Writer _write;
    uint32_t _passed{0};
    uint32_t _failed{0};
};

}  // namespace testing

// A test is a free function taking the context by reference as `_t`; the
// ASSERT_/EXPECT_ macros act on that implicit `_t`.
#define TEST(name) void name(::testing::TestContext &_t)

#define STM32_TEST_FAIL_(expr)                     \
    do {                                           \
        _t.failed = true;                          \
        ::testing::reportFailure(_t, (expr));      \
    } while (0)

#define EXPECT_TRUE(cond)                          \
    do {                                           \
        ++_t.checks;                               \
        if (!(cond)) {                             \
            STM32_TEST_FAIL_(#cond);               \
        }                                          \
    } while (0)

#define EXPECT_FALSE(cond)                         \
    do {                                           \
        ++_t.checks;                               \
        if (cond) {                                \
            STM32_TEST_FAIL_("!(" #cond ")");      \
        }                                          \
    } while (0)

#define EXPECT_EQ(lhs, rhs)                        \
    do {                                           \
        ++_t.checks;                               \
        if (!((lhs) == (rhs))) {                   \
            STM32_TEST_FAIL_(#lhs " == " #rhs);    \
        }                                          \
    } while (0)

#define EXPECT_NE(lhs, rhs)                        \
    do {                                           \
        ++_t.checks;                               \
        if (!((lhs) != (rhs))) {                   \
            STM32_TEST_FAIL_(#lhs " != " #rhs);    \
        }                                          \
    } while (0)

#define ASSERT_TRUE(cond)                          \
    do {                                           \
        ++_t.checks;                               \
        if (!(cond)) {                             \
            STM32_TEST_FAIL_(#cond);               \
            return;                                \
        }                                          \
    } while (0)

#define ASSERT_FALSE(cond)                         \
    do {                                           \
        ++_t.checks;                               \
        if (cond) {                                \
            STM32_TEST_FAIL_("!(" #cond ")");      \
            return;                                \
        }                                          \
    } while (0)

#define ASSERT_EQ(lhs, rhs)                        \
    do {                                           \
        ++_t.checks;                               \
        if (!((lhs) == (rhs))) {                   \
            STM32_TEST_FAIL_(#lhs " == " #rhs);    \
            return;                                \
        }                                          \
    } while (0)

#define ASSERT_NE(lhs, rhs)                        \
    do {                                           \
        ++_t.checks;                               \
        if (!((lhs) != (rhs))) {                   \
            STM32_TEST_FAIL_(#lhs " != " #rhs);    \
            return;                                \
        }                                          \
    } while (0)

#define RUN_TEST(runner, name) (runner).run(#name, &name)

#endif  // STM32_SDK_TESTING_UNIT_TEST_HPP
