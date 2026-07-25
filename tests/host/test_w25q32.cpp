#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.types;
import testing.mock;
import sensor.w25q32;

using driver::Status;

namespace {
void put(const char *s) {
  std::fputs(s, stdout);
}
constexpr uint32_t kJedec = sensor::W25q32Spec::JEDEC_W25Q32JV;

sensor::W25q32<testing::MockSpi, testing::MockGpioPin>
makeFlash(testing::MockSpi &spi, testing::MockGpioPin &cs) {
  return sensor::W25q32{
      spi,
      cs,
      {.expectedJedecId = kJedec, .busyPollLoops = 1000}
  };
}
}  // namespace

TEST(w25q32_init_reads_jedec_id) {
  testing::MockSpi spi;
  testing::MockGpioPin cs;
  const uint8_t id[3] = {0xEF, 0x40, 0x16};
  spi.loadResponse({id, 3});
  auto flash = makeFlash(spi, cs);

  EXPECT_EQ(flash.init(), Status::Ok);
  EXPECT_EQ(flash.jedecId(), kJedec);

  auto w = spi.written();
  ASSERT_TRUE(w.size() >= static_cast<size_t>(1));
  EXPECT_EQ(w[0], 0x9F);  // JEDEC_ID command
}

TEST(w25q32_init_rejects_wrong_jedec) {
  testing::MockSpi spi;
  testing::MockGpioPin cs;
  const uint8_t id[3] = {0x12, 0x34, 0x56};
  spi.loadResponse({id, 3});
  auto flash = makeFlash(spi, cs);

  EXPECT_EQ(flash.init(), Status::HardwareError);
}

TEST(w25q32_write_page_rejects_out_of_bounds) {
  testing::MockSpi spi;
  testing::MockGpioPin cs;
  auto flash = makeFlash(spi, cs);

  uint8_t buf[300] = {0};
  // Larger than PAGE_SIZE (256).
  EXPECT_EQ(flash.writePage(0, {buf, 300}), Status::InvalidArg);
  // Crosses a page boundary: 250 + 10 > 256.
  EXPECT_EQ(flash.writePage(250, {buf, 10}), Status::InvalidArg);
  // Empty write is a no-op.
  EXPECT_EQ(flash.writePage(0, {buf, 0}), Status::Ok);
}

TEST(w25q32_read_emits_address_header) {
  testing::MockSpi spi;
  testing::MockGpioPin cs;
  const uint8_t payload[2] = {0xAB, 0xCD};
  spi.loadResponse({payload, 2});
  auto flash = makeFlash(spi, cs);

  uint8_t out[2] = {0, 0};
  EXPECT_EQ(flash.read(0x123456, {out, 2}), Status::Ok);

  auto w = spi.written();
  ASSERT_EQ(w.size(), static_cast<size_t>(4));
  EXPECT_EQ(w[0], 0x03);  // READ_DATA opcode
  EXPECT_EQ(w[1], 0x12);
  EXPECT_EQ(w[2], 0x34);
  EXPECT_EQ(w[3], 0x56);
  EXPECT_EQ(out[0], 0xAB);
  EXPECT_EQ(out[1], 0xCD);
}

TEST(w25q32_geometry_constants) {
  EXPECT_EQ(sensor::W25q32Spec::PAGE_SIZE, static_cast<uint32_t>(256));
  EXPECT_EQ(sensor::W25q32Spec::SECTOR_SIZE, static_cast<uint32_t>(4096));
  EXPECT_EQ(
      sensor::W25q32Spec::JEDEC_W25Q32JV,
      static_cast<uint32_t>(0xEF4016)
  );
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, w25q32_init_reads_jedec_id);
  RUN_TEST(runner, w25q32_init_rejects_wrong_jedec);
  RUN_TEST(runner, w25q32_write_page_rejects_out_of_bounds);
  RUN_TEST(runner, w25q32_read_emits_address_header);
  RUN_TEST(runner, w25q32_geometry_constants);
  return runner.summary() ? 0 : 1;
}
