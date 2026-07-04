module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.spi;

import driver.types;

export namespace driver {

enum class SpiMode : uint8_t {
  None = 0xFF,
  Mode0 = 0,
  Mode1 = 1,
  Mode2 = 2,
  Mode3 = 3
};

enum class SpiDataSize : uint8_t {
  None = 0xFF,
  Bits8 = 8,
  Bits16 = 16
};

struct SpiConfig {
  uint32_t clockHz;
  SpiMode mode;
  bool lsbFirst;
  SpiDataSize dataSize;
};

consteval SpiConfig spi(SpiConfig c) {
  if (c.clockHz == 0) {
    throw "SpiConfig: clockHz must be > 0";
  }
  if (c.mode == SpiMode::None) {
    throw "SpiConfig: mode must be set (Mode0..Mode3)";
  }
  if (c.dataSize == SpiDataSize::None) {
    throw "SpiConfig: dataSize must be set (Bits8 or Bits16)";
  }
  return c;
}

// Compile-time contract for an SPI master bus (replaces the former virtual
// ISpi base class).
template <typename T>
concept ISpi =
    requires(T bus, std::span<const uint8_t> tx, std::span<uint8_t> rx) {
      { bus.transfer(tx, rx) } -> std::same_as<Status>;
      { bus.write(tx) } -> std::same_as<Status>;
      { bus.read(rx) } -> std::same_as<Status>;
    };

}  // namespace driver
