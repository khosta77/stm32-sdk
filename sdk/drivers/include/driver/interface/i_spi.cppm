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
  // Bits16 is reserved but not yet implemented: the STM32F4 driver drives DR
  // one byte at a time over span<uint8_t>, so a 16-bit frame size would corrupt
  // framing. spi() rejects it at compile time until the driver gains a 16-bit
  // path. See issue tracker.
  Bits16 = 16
};

struct SpiConfig {
  uint32_t clockHz;
  SpiMode mode;
  bool lsbFirst;
  SpiDataSize dataSize;
};

template <SpiConfig C>
consteval SpiConfig spi() {
  static_assert(C.clockHz != 0, "SpiConfig: clockHz must be > 0");
  static_assert(
      C.mode != SpiMode::None,
      "SpiConfig: mode must be set (Mode0..Mode3)"
  );
  static_assert(
      C.dataSize != SpiDataSize::None,
      "SpiConfig: dataSize must be set (currently only Bits8)"
  );
  static_assert(
      C.dataSize != SpiDataSize::Bits16,
      "SpiConfig: dataSize Bits16 is not supported yet (driver is 8-bit PIO); "
      "use SpiDataSize::Bits8"
  );
  return C;
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
