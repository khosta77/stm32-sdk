#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.log;

// Compile every level in so the runtime gate is what we exercise here; the
// compile-time strip (#if STM32_LOG_LEVEL) is proven by the firmware release
// build, not reachable at runtime.
#define STM32_LOG_LEVEL 5
#include "driver/log.hpp"

namespace {

// Capturing sink: appends every byte run so a test can inspect the exact bytes
// the facility emitted.
char g_buf[512];
size_t g_len = 0;

void captureSink(void * /*ctx*/, const char *data, size_t len) {
  for (size_t i = 0; i < len && g_len + 1 < sizeof(g_buf); ++i) {
    g_buf[g_len++] = data[i];
  }
  g_buf[g_len] = '\0';
}

void reset() {
  g_len = 0;
  g_buf[0] = '\0';
}

bool captured(const char *expected) {
  return __builtin_strcmp(g_buf, expected) == 0;
}

void put(const char *s) {
  std::fputs(s, stdout);
}

}  // namespace

TEST(message_is_framed_with_level_and_tag) {
  driver::log::setSink(&captureSink, nullptr);
  driver::log::setLevel(driver::log::Level::Trace);
  reset();
  LOG_INFO("app", "ready");
  EXPECT_TRUE(captured("I [app] ready\r\n"));
}

TEST(runtime_level_gates_lower_priority) {
  driver::log::setSink(&captureSink, nullptr);
  driver::log::setLevel(driver::log::Level::Warn);
  reset();
  LOG_INFO("app", "dropped");
  LOG_DEBUG("app", "dropped");
  EXPECT_EQ(g_len, static_cast<size_t>(0));
  LOG_WARN("app", "kept");
  LOG_ERROR("app", "kept");
  EXPECT_TRUE(captured("W [app] kept\r\nE [app] kept\r\n"));
}

TEST(level_none_silences_everything) {
  driver::log::setSink(&captureSink, nullptr);
  driver::log::setLevel(driver::log::Level::None);
  reset();
  LOG_ERROR("app", "silent");
  EXPECT_EQ(g_len, static_cast<size_t>(0));
}

TEST(u32_value_is_appended_as_decimal) {
  driver::log::setSink(&captureSink, nullptr);
  driver::log::setLevel(driver::log::Level::Trace);
  reset();
  LOG_INFO_U32("adc", "raw=", 4294967295U);
  EXPECT_TRUE(captured("I [adc] raw=4294967295\r\n"));
}

TEST(hex_value_is_prefixed_and_lowercase) {
  driver::log::setSink(&captureSink, nullptr);
  driver::log::setLevel(driver::log::Level::Trace);
  reset();
  LOG_DEBUG_HEX("reg", "cr1=", 0xDEADBEEFU);
  EXPECT_TRUE(captured("D [reg] cr1=0xdeadbeef\r\n"));
}

TEST(no_sink_is_safe) {
  driver::log::setSink(nullptr, nullptr);
  driver::log::setLevel(driver::log::Level::Trace);
  LOG_ERROR("app", "no crash");
  EXPECT_TRUE(true);
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, message_is_framed_with_level_and_tag);
  RUN_TEST(runner, runtime_level_gates_lower_priority);
  RUN_TEST(runner, level_none_silences_everything);
  RUN_TEST(runner, u32_value_is_appended_as_decimal);
  RUN_TEST(runner, hex_value_is_prefixed_and_lowercase);
  RUN_TEST(runner, no_sink_is_safe);
  return runner.summary() ? 0 : 1;
}
