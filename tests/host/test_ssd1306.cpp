#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.types;
import testing.mock;
import sensor.ssd1306;

using driver::Status;

namespace {
void put(const char *s) {
  std::fputs(s, stdout);
}

sensor::Ssd1306<testing::MockI2c> makeDisplay(testing::MockI2c &bus) {
  return sensor::Ssd1306{
      bus,
      {.addr = 0x3C, .contrast = 0x7F, .flipH = false, .flipV = false}
  };
}
}  // namespace

TEST(ssd1306_init_and_geometry) {
  testing::MockI2c bus;
  auto disp = makeDisplay(bus);

  EXPECT_EQ(disp.init(), Status::Ok);
  EXPECT_EQ(disp.width(), static_cast<uint16_t>(128));
  EXPECT_EQ(disp.height(), static_cast<uint16_t>(64));
}

TEST(ssd1306_flush_emits_addr_window_and_pixel) {
  testing::MockI2c bus;
  auto disp = makeDisplay(bus);

  disp.clear();
  disp.setPixel(3, 2, true);  // framebuffer byte 3, bit 1<<2 = 0x04
  EXPECT_EQ(disp.flush(), Status::Ok);

  // Capture layout: six command writes (each [CMD_STREAM, byte]) then one data
  // write ([DATA_STREAM, framebuffer...]).
  auto w = bus.written();
  ASSERT_TRUE(w.size() >= static_cast<size_t>(17));
  EXPECT_EQ(w[0], 0x00);   // CMD_STREAM control byte
  EXPECT_EQ(w[1], 0x21);   // COLUMN_ADDR
  EXPECT_EQ(w[5], 0x7F);   // WIDTH - 1 = 127
  EXPECT_EQ(w[7], 0x22);   // PAGE_ADDR
  EXPECT_EQ(w[11], 0x07);  // PAGES - 1 = 7
  EXPECT_EQ(w[12], 0x40);  // DATA_STREAM control byte
  EXPECT_EQ(w[16], 0x04);  // framebuffer[3] = bit set by setPixel(3, 2)
}

TEST(ssd1306_setpixel_out_of_bounds_is_noop) {
  testing::MockI2c bus;
  auto disp = makeDisplay(bus);

  disp.clear();
  disp.setPixel(200, 200, true);  // out of 128x64 range: ignored
  EXPECT_EQ(disp.flush(), Status::Ok);

  auto w = bus.written();
  bool anyData = false;
  for (size_t i = 13; i < w.size(); ++i) {
    if (w[i] != 0) {
      anyData = true;
    }
  }
  EXPECT_FALSE(anyData);  // framebuffer stayed all-zero
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, ssd1306_init_and_geometry);
  RUN_TEST(runner, ssd1306_flush_emits_addr_window_and_pixel);
  RUN_TEST(runner, ssd1306_setpixel_out_of_bounds_is_noop);
  return runner.summary() ? 0 : 1;
}
