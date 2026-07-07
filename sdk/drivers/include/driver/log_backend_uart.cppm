module;
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.log.uart;

import driver.log;
import driver.uart;

// UART log backend (issue #37). Forwards formatted bytes to any driver::IUart
// implementation, so logs land on the same serial line used for application
// output. Generic over the bus in the terse constrained style used across the
// SDK; the captured UartDriver& travels through the sink's ctx pointer.

export namespace driver::log {

template <driver::IUart UartDriver>
class UartBackend {
public:
  explicit UartBackend(UartDriver &uart) : _uart(uart) {}

  // Routes driver.log output to the wrapped UART.
  void install() { setSink(&UartBackend::sink, this); }

private:
  static void sink(void *ctx, const char *data, size_t len) {
    auto *self = static_cast<UartBackend *>(ctx);
    (void) self->_uart.write({reinterpret_cast<const uint8_t *>(data), len});
  }

  UartDriver &_uart;
};

}  // namespace driver::log
