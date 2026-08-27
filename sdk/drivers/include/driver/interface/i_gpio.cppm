module;
#include <concepts>
#include <cstdint>
export module driver.gpio;

import driver.types;

export namespace driver {

enum class PinMode : uint8_t {
  None = 0xFF,
  Input = 0,
  Output = 1,
  AlternateFunction = 2,
  Analog = 3
};

enum class PullMode : uint8_t {
  None = 0,
  PullUp = 1,
  PullDown = 2
};

enum class OutputSpeed : uint8_t {
  None = 0xFF,
  Low = 0,
  Medium = 1,
  High = 2,
  VeryHigh = 3
};

enum class OutputType : uint8_t {
  None = 0xFF,
  PushPull = 0,
  OpenDrain = 1
};

struct GpioConfig {
  uint8_t pin;
  PinMode mode;
  PullMode pull;
  OutputSpeed speed;
  OutputType type;
  uint8_t af = 0;
};

template <GpioConfig C>
consteval GpioConfig gpio() {
  constexpr bool needsDrive =
      (C.mode == PinMode::Output || C.mode == PinMode::AlternateFunction);

  static_assert(C.pin <= 15, "GpioConfig: pin must be in [0, 15]");
  static_assert(
      C.mode != PinMode::None,
      "GpioConfig: mode must not be PinMode::None"
  );
  static_assert(
      !needsDrive || C.speed != OutputSpeed::None,
      "GpioConfig: OutputSpeed required for Output/AlternateFunction"
  );
  static_assert(
      !needsDrive || C.type != OutputType::None,
      "GpioConfig: OutputType required for Output/AlternateFunction"
  );
  static_assert(
      C.mode != PinMode::AlternateFunction || C.af <= 15,
      "GpioConfig: af must be in [0, 15]"
  );
  return C;
}

// Compile-time contract for a single GPIO pin (replaces the former virtual
// IGpioPin base class; satisfied by GpioPin / NullGpioPin without inheritance).
template <typename T>
concept IGpioPin = requires(T pin) {
  { pin.set() } -> std::same_as<void>;
  { pin.reset() } -> std::same_as<void>;
  { pin.toggle() } -> std::same_as<void>;
  { pin.read() } -> std::same_as<Status>;
};

}  // namespace driver
