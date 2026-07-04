module;
#include <cstdint>
#include "cmsis/stm32f4xx.h"
export module driver.stm32f4.gpio;

import driver.gpio;
import driver.types;
import driver.reg;

export namespace driver {
namespace stm32f4 {

class GpioPin {
  GPIO_TypeDef &_port;
  uint8_t _pin;

public:
  GpioPin(GPIO_TypeDef &port, const GpioConfig &cfg)
      : _port(port), _pin(cfg.pin) {
    if (&_port == GPIOA) {
      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    } else if (&_port == GPIOB) {
      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    } else if (&_port == GPIOC) {
      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    } else if (&_port == GPIOD) {
      RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    } else if (&_port == GPIOE) {
      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    }
    __DSB();

    uint32_t pos2 = _pin * 2U;
    reg::modify(
        _port.MODER,
        0x3U << pos2,
        static_cast<uint32_t>(cfg.mode) << pos2
    );
    reg::modify(
        _port.PUPDR,
        0x3U << pos2,
        static_cast<uint32_t>(cfg.pull) << pos2
    );

    uint32_t speedVal = (cfg.speed != OutputSpeed::None)
                            ? static_cast<uint32_t>(cfg.speed)
                            : 0U;
    reg::modify(_port.OSPEEDR, 0x3U << pos2, speedVal << pos2);

    if (cfg.type == OutputType::OpenDrain) {
      reg::set(_port.OTYPER, 1U << _pin);
    } else {
      reg::clear(_port.OTYPER, 1U << _pin);
    }

    if (cfg.mode == PinMode::AlternateFunction) {
      uint32_t afIdx = (_pin < 8) ? 0U : 1U;
      uint32_t afPos = (_pin & 0x7U) * 4U;
      reg::modify(
          _port.AFR[afIdx],
          0xFU << afPos,
          static_cast<uint32_t>(cfg.af) << afPos
      );
    }
  }

  void set() { _port.BSRR = (1U << _pin); }

  void reset() { _port.BSRR = (1U << (_pin + 16U)); }

  void toggle() {
    if (reg::read(_port.ODR, 1U << _pin)) {
      _port.BSRR = (1U << (_pin + 16U));
    } else {
      _port.BSRR = (1U << _pin);
    }
  }

  [[nodiscard]] Status read() {
    return reg::read(_port.IDR, 1U << _pin) ? Status::Ok : Status::None;
  }
};

static_assert(IGpioPin<GpioPin>, "GpioPin must model driver::IGpioPin");

}  // namespace stm32f4
}  // namespace driver
