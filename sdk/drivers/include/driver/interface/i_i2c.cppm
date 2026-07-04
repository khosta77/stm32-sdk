module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.i2c;

import driver.types;

export namespace driver {

struct I2cConfig {
  uint32_t clockSpeed;
  bool fastMode;
};

consteval I2cConfig i2c(I2cConfig c) {
  if (c.clockSpeed == 0 || c.clockSpeed > 400000) {
    throw "I2cConfig: clockSpeed must be in [1, 400000]";
  }
  if (!c.fastMode && c.clockSpeed > 100000) {
    throw "I2cConfig: clockSpeed > 100000 requires fastMode";
  }
  return c;
}

// Compile-time contract for an I2C master bus (replaces the former virtual
// II2c base class). A sensor templated on `II2c Bus` calls these directly.
template <typename T>
concept II2c = requires(
    T bus,
    uint8_t addr,
    uint8_t reg,
    std::span<const uint8_t> wr,
    std::span<uint8_t> rd
) {
  { bus.write(addr, wr) } -> std::same_as<Status>;
  { bus.read(addr, rd) } -> std::same_as<Status>;
  { bus.writeReg(addr, reg, wr) } -> std::same_as<Status>;
  { bus.readReg(addr, reg, rd) } -> std::same_as<Status>;
  { bus.probe(addr) } -> std::same_as<Status>;
};

}  // namespace driver
