#include <cstdint>
#include "cmsis/stm32f4xx.h"

import driver.gpio;
import driver.stm32f4.gpio;

using driver::gpio;
using driver::OutputSpeed;
using driver::OutputType;
using driver::PinMode;
using driver::PullMode;
using driver::stm32f4::GpioPin;

namespace {

volatile uint32_t g_ticks = 0;

// The four on-board LEDs on the STM32F4-Discovery (PD12..PD15). Each GpioPin
// enables the GPIOD clock and configures the pin in its constructor, so no
// manual RCC/MODER poking is needed.
GpioPin g_green{
    *GPIOD,
    gpio({
        .pin = 12,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
GpioPin g_orange{
    *GPIOD,
    gpio({
        .pin = 13,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
GpioPin g_red{
    *GPIOD,
    gpio({
        .pin = 14,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
GpioPin g_blue{
    *GPIOD,
    gpio({
        .pin = 15,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};

void delay(uint32_t ms) {
  uint32_t start = g_ticks;
  while (g_ticks - start < ms) {
  }
}

}  // namespace

extern "C" void SysTick_Handler() {
  g_ticks = g_ticks + 1;
}

int main() {
  SystemCoreClockUpdate();
  SysTick_Config(SystemCoreClock / 1000);

  for (;;) {
    g_green.set();
    delay(125);
    g_orange.set();
    delay(125);
    g_red.set();
    delay(125);
    g_blue.set();
    delay(125);

    g_green.reset();
    g_orange.reset();
    g_red.reset();
    g_blue.reset();
    delay(500);
  }
}
