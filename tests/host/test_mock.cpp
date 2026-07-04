#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.types;
import testing.mock;

using driver::Status;

namespace {
void put(const char *s) { std::fputs(s, stdout); }
}  // namespace

TEST(mock_i2c_scripts_reads_and_captures_writes) {
  testing::MockI2c bus;
  const uint8_t resp[3] = {0xAA, 0xBB, 0xCC};
  bus.loadResponse({resp, 3});

  uint8_t out[3] = {0, 0, 0};
  EXPECT_EQ(bus.read(0x50, {out, 3}), Status::Ok);
  EXPECT_EQ(out[0], 0xAA);
  EXPECT_EQ(out[2], 0xCC);

  const uint8_t payload[2] = {0x11, 0x22};
  EXPECT_EQ(bus.writeReg(0x50, 0x0F, {payload, 2}), Status::Ok);
  auto written = bus.written();
  ASSERT_EQ(written.size(), static_cast<size_t>(3));
  EXPECT_EQ(written[0], 0x0F);
  EXPECT_EQ(written[1], 0x11);
  EXPECT_EQ(written[2], 0x22);
  EXPECT_EQ(bus.lastAddr, 0x50);
  EXPECT_EQ(bus.lastReg, 0x0F);
}

TEST(mock_i2c_injects_status) {
  testing::MockI2c bus;
  bus.nextStatus = Status::Nack;
  uint8_t out[1] = {0};
  EXPECT_EQ(bus.read(0x10, {out, 1}), Status::Nack);
  EXPECT_EQ(bus.probe(0x10), Status::Nack);
  EXPECT_EQ(bus.probeCount, static_cast<uint32_t>(1));
}

TEST(mock_gpio_tracks_level_and_counts) {
  testing::MockGpioPin pin;
  pin.set();
  pin.toggle();
  pin.reset();
  EXPECT_EQ(pin.setCount, static_cast<uint32_t>(1));
  EXPECT_EQ(pin.toggleCount, static_cast<uint32_t>(1));
  EXPECT_EQ(pin.resetCount, static_cast<uint32_t>(1));
  EXPECT_EQ(pin.read(), Status::None);
  pin.set();
  EXPECT_EQ(pin.read(), Status::Ok);
}

TEST(mock_flash_write_read_erase) {
  testing::MockFlash flash;
  const uint8_t data[4] = {1, 2, 3, 4};
  EXPECT_EQ(flash.write(0, {data, 4}), Status::Ok);

  uint8_t rb[4] = {0, 0, 0, 0};
  EXPECT_EQ(flash.read(0, {rb, 4}), Status::Ok);
  EXPECT_EQ(rb[0], 1);
  EXPECT_EQ(rb[3], 4);

  EXPECT_EQ(flash.eraseSector(0), Status::Ok);
  EXPECT_EQ(flash.read(0, {rb, 4}), Status::Ok);
  EXPECT_EQ(rb[0], 0xFF);
  EXPECT_EQ(flash.sectorSize(0), static_cast<size_t>(64));
  EXPECT_EQ(flash.sectorCount(), static_cast<uint8_t>(4));
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, mock_i2c_scripts_reads_and_captures_writes);
  RUN_TEST(runner, mock_i2c_injects_status);
  RUN_TEST(runner, mock_gpio_tracks_level_and_counts);
  RUN_TEST(runner, mock_flash_write_read_erase);
  return runner.summary() ? 0 : 1;
}
