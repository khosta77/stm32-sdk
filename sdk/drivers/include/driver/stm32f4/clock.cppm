module;
#include <cstdint>
#include "cmsis/stm32f4xx.h"
export module driver.stm32f4.clock;

import driver.reg;

export namespace driver {
namespace stm32f4 {

[[nodiscard]] inline uint32_t getApb1Clock() {
  uint32_t ppre1 = (reg::get(RCC->CFGR) >> RCC_CFGR_PPRE1_Pos) & 0x7U;
  uint32_t div = (ppre1 < 4U) ? 1U : (2U << (ppre1 - 4U));
  return SystemCoreClock / div;
}

[[nodiscard]] inline uint32_t getApb2Clock() {
  uint32_t ppre2 = (reg::get(RCC->CFGR) >> RCC_CFGR_PPRE2_Pos) & 0x7U;
  uint32_t div = (ppre2 < 4U) ? 1U : (2U << (ppre2 - 4U));
  return SystemCoreClock / div;
}

[[nodiscard]] inline uint32_t getTimeoutLoops() {
  return SystemCoreClock / 10;
}

}  // namespace stm32f4
}  // namespace driver
