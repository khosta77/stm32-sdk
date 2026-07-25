module;
#include <concepts>
#include <cstdint>
export module sensor.imu;

import driver.types;

export namespace sensor {

struct Vec3f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct ImuData {
  Vec3f accel;
  Vec3f gyro;
  float temp = 0.0f;
};

// Compile-time contract for an IMU (replaces the former virtual IImu base
// class). A concrete sensor models it without inheritance.
template <typename T>
concept IImu = requires(T sensor, ImuData &out, uint8_t g, uint16_t dps) {
  { sensor.init() } -> std::same_as<driver::Status>;
  { sensor.read(out) } -> std::same_as<driver::Status>;
  { sensor.selfTest() } -> std::same_as<driver::Status>;
  { sensor.setAccelRange(g) } -> std::same_as<driver::Status>;
  { sensor.setGyroRange(dps) } -> std::same_as<driver::Status>;
};

}  // namespace sensor
