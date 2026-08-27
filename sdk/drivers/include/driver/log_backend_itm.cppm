module;
#include <cstddef>
#include <cstdint>
#include "cmsis_device.h"
export module driver.log.itm;

import driver.log;

// SWO/ITM log backend (issue #37). Streams every byte to ITM stimulus port 0,
// read back over SWO by the debugger (openocd `itm port 0 on`, ST-Link SWV).
//
// ITM_SendChar is a CMSIS __STATIC_INLINE (TU-local) core function; ItmBackend
// is non-template, so it calls it directly -- same as GpioPin / ExtiLine call
// NVIC_* without detail:: wrappers. If the debugger has not enabled trace,
// ITM_SendChar is a no-op, so this is safe to install unconditionally.

export namespace driver::log {

class ItmBackend {
public:
  // Routes driver.log output to ITM port 0. Stateless; ctx is unused.
  void install() { setSink(&ItmBackend::sink, nullptr); }

private:
  static void sink(void * /*ctx*/, const char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      ITM_SendChar(static_cast<uint32_t>(static_cast<uint8_t>(data[i])));
    }
  }
};

}  // namespace driver::log
