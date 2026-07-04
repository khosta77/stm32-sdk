module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.uart;

import driver.types;

export namespace driver {

enum class Parity : uint8_t {
  None,
  Even,
  Odd
};

enum class DataBits : uint8_t {
  None = 0xFF,
  Eight = 8,
  Nine = 9
};

enum class StopBits : uint8_t {
  None = 0xFF,
  One = 1,
  Two = 2
};

struct UartConfig {
  uint32_t baudrate;
  DataBits dataBits;
  StopBits stopBits;
  Parity parity;
};

consteval UartConfig uart(UartConfig c) {
  if (c.baudrate == 0) {
    throw "UartConfig: baudrate must be > 0";
  }
  if (c.dataBits == DataBits::None) {
    throw "UartConfig: dataBits must be set (Eight or Nine)";
  }
  if (c.stopBits == StopBits::None) {
    throw "UartConfig: stopBits must be set (One or Two)";
  }
  return c;
}

// Compile-time contract for a UART (replaces the former virtual IUart base
// class). `rxAvailable` / `txFree` are checked through a const object.
template <typename T>
concept IUart = requires(
    T uart,
    const T cuart,
    std::span<const uint8_t> wr,
    std::span<uint8_t> rd
) {
  { uart.write(wr) } -> std::same_as<size_t>;
  { uart.read(rd) } -> std::same_as<size_t>;
  { uart.writeNonBlocking(wr) } -> std::same_as<size_t>;
  { uart.readNonBlocking(rd) } -> std::same_as<size_t>;
  { cuart.rxAvailable() } -> std::same_as<size_t>;
  { cuart.txFree() } -> std::same_as<size_t>;
  { uart.irqHandler() } -> std::same_as<void>;
};

}  // namespace driver
