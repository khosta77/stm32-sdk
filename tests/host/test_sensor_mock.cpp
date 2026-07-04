#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import driver.types;
import sensor.imu;
import sensor.mpu6050;
import testing.mock;

using driver::Status;

namespace {

constexpr sensor::Mpu6050<testing::MockI2c>::Config kCfg{
    .addr = 0x68,
    .accelRange = 0,
    .gyroRange = 250,
    .sampleRateDiv = 0,
    .dlpfMode = 0,
};

void put(const char *s) { std::fputs(s, stdout); }

}  // namespace

TEST(mpu6050_read_parses_and_scales_accel) {
  testing::MockI2c bus;
  // ACCEL_XOUT: hi=0x40 lo=0x00 -> raw 16384 -> /16384 * 9.80665 = 1 g.
  const uint8_t frame[14] = {0x40, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  bus.loadResponse({frame, 14});

  sensor::Mpu6050 mpu{bus, kCfg};
  sensor::ImuData data;
  ASSERT_EQ(mpu.read(data), Status::Ok);
  EXPECT_EQ(bus.lastReg, 0x3B);  // ACCEL_XOUT_H: driver addressed the right reg
  EXPECT_TRUE(data.accel.x > 9.8f && data.accel.x < 9.82f);
  EXPECT_TRUE(data.accel.y == 0.0f);
}

TEST(mpu6050_read_propagates_bus_error) {
  testing::MockI2c bus;
  bus.nextStatus = Status::Timeout;
  sensor::Mpu6050 mpu{bus, kCfg};
  sensor::ImuData data;
  EXPECT_EQ(mpu.read(data), Status::Timeout);
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, mpu6050_read_parses_and_scales_accel);
  RUN_TEST(runner, mpu6050_read_propagates_bus_error);
  return runner.summary() ? 0 : 1;
}
