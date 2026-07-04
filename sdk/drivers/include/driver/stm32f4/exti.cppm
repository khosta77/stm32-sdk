module;
#include <cstdint>
#include "cmsis/stm32f4xx.h"
export module driver.stm32f4.exti;

import driver.exti;
import driver.reg;

export namespace driver {
namespace stm32f4 {

class ExtiLine {
public:
  using Thunk = void (*)(void *);

  ExtiLine(const ExtiConfig &cfg, Thunk callback, void *ctx)
      : _mask(1U << cfg.line),
        _irqn(lineIrqn(cfg.line)),
        _callback(callback),
        _ctx(ctx) {
    reg::set(RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN);
    __DSB();

    const uint32_t idx = cfg.line >> 2U;
    const uint32_t pos = (cfg.line & 0x3U) * 4U;
    reg::modify(
        SYSCFG->EXTICR[idx],
        0xFU << pos,
        static_cast<uint32_t>(cfg.port) << pos
    );

    const bool rising = cfg.trigger != ExtiTrigger::Falling;
    const bool falling = cfg.trigger != ExtiTrigger::Rising;
    if (rising) {
      reg::set(EXTI->RTSR, _mask);
    } else {
      reg::clear(EXTI->RTSR, _mask);
    }
    if (falling) {
      reg::set(EXTI->FTSR, _mask);
    } else {
      reg::clear(EXTI->FTSR, _mask);
    }

    reg::write(EXTI->PR, _mask);
    NVIC_SetPriority(_irqn, cfg.priority);
    NVIC_EnableIRQ(_irqn);
    enable();
  }

  ExtiLine(const ExtiLine &) = delete;
  ExtiLine &operator=(const ExtiLine &) = delete;

  template <auto Method, class T>
  [[nodiscard]] static ExtiLine bind(const ExtiConfig &cfg, T &obj) {
    return ExtiLine(cfg, &memberThunk<Method, T>, &obj);
  }

  void enable() { reg::set(EXTI->IMR, _mask); }
  void disable() { reg::clear(EXTI->IMR, _mask); }
  void clearPending() { reg::write(EXTI->PR, _mask); }

  [[nodiscard]] bool pending() const { return reg::read(EXTI->PR, _mask); }

  void irqHandler() {
    if (reg::read(EXTI->PR, _mask)) {
      reg::write(EXTI->PR, _mask);
      if (_callback != nullptr) {
        _callback(_ctx);
      }
    }
  }

private:
  template <auto Method, class T>
  static void memberThunk(void *ctx) {
    (static_cast<T *>(ctx)->*Method)();
  }

  static IRQn_Type lineIrqn(uint8_t line) {
    switch (line) {
      case 0:
        return EXTI0_IRQn;
      case 1:
        return EXTI1_IRQn;
      case 2:
        return EXTI2_IRQn;
      case 3:
        return EXTI3_IRQn;
      case 4:
        return EXTI4_IRQn;
      default:
        break;
    }
    return (line <= 9) ? EXTI9_5_IRQn : EXTI15_10_IRQn;
  }

  uint32_t _mask;
  IRQn_Type _irqn;
  Thunk _callback;
  void *_ctx;
};

static_assert(IExti<ExtiLine>, "ExtiLine must model driver::IExti");

}  // namespace stm32f4
}  // namespace driver
