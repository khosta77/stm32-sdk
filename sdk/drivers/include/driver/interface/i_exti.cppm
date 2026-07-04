module;
#include <concepts>
#include <cstdint>
export module driver.exti;

export namespace driver {

enum class ExtiTrigger : uint8_t {
  Rising = 0,
  Falling = 1,
  Both = 2,
};

// Port selector for the SYSCFG_EXTICR nibble: A=0 .. H=7. The value is written
// verbatim into the 4-bit field that routes GPIOx[pin] to EXTI line `pin`.
enum class ExtiPort : uint8_t {
  A = 0,
  B = 1,
  C = 2,
  D = 3,
  E = 4,
  F = 5,
  G = 6,
  H = 7,
};

struct ExtiConfig {
  uint8_t line;
  ExtiPort port;
  ExtiTrigger trigger;
  uint32_t priority;
};

consteval ExtiConfig exti(ExtiConfig c) {
  if (c.line > 15) {
    throw "ExtiConfig: line must be in [0, 15]";
  }
  if (c.priority > 15) {
    throw "ExtiConfig: priority must be in [0, 15] (4 NVIC priority bits on "
          "F4)";
  }
  return c;
}

// Compile-time contract for a single external-interrupt line. Satisfied by
// driver::stm32f4::ExtiLine without inheritance (same concept style as
// IGpioPin).
template <typename T>
concept IExti = requires(T e) {
  { e.enable() } -> std::same_as<void>;
  { e.disable() } -> std::same_as<void>;
  { e.clearPending() } -> std::same_as<void>;
  { e.pending() } -> std::same_as<bool>;
  { e.irqHandler() } -> std::same_as<void>;
};

}  // namespace driver
