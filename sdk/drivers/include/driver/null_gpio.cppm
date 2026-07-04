module;
export module driver.null_gpio;

import driver.gpio;
import driver.types;

export namespace driver {

// No-op pin for boards where a line (e.g. SPI CS) is hardwired. Models the
// driver::IGpioPin concept as a plain struct — no inheritance, no vtable.
struct NullGpioPin {
  void set() {}
  void reset() {}
  void toggle() {}
  [[nodiscard]] Status read() { return Status::None; }
};

static_assert(IGpioPin<NullGpioPin>, "NullGpioPin must model driver::IGpioPin");

}  // namespace driver
