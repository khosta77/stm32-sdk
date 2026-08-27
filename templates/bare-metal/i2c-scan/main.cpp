#include <cstddef>
#include <cstdint>
#include "cmsis/stm32f4xx.h"

import driver.types;
import driver.gpio;
import driver.i2c;
import driver.reg;
import driver.stm32f4.clock;
import driver.stm32f4.gpio;
import driver.stm32f4.i2c;

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::gpio;
using driver::GpioConfig;
using driver::i2c;
using driver::OutputSpeed;
using driver::OutputType;
using driver::PinMode;
using driver::PullMode;
using driver::stm32f4::GpioPin;
using driver::stm32f4::I2c;

extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN);
  driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN);
  __DSB();
}

namespace {

GpioPin g_uartTx{
    *GPIOA,
    gpio<{
        .pin = 2,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 7,
    }>(),
};
GpioPin g_uartRx{
    *GPIOA,
    gpio<{
        .pin = 3,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 7,
    }>(),
};
GpioPin g_i2cScl{
    *GPIOB,
    gpio<{
        .pin = 6,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::PullUp,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::OpenDrain,
        .af = 4,
    }>(),
};
GpioPin g_i2cSda{
    *GPIOB,
    gpio<{
        .pin = 7,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::PullUp,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::OpenDrain,
        .af = 4,
    }>(),
};

I2c g_i2c1{
    *I2C1,
    i2c<{
        .clockSpeed = 100000,
        .fastMode = false,
    }>(),
};

void uartInit() {
  const uint32_t pclk = driver::stm32f4::getApb1Clock();
  driver::reg::write(USART2->BRR, (pclk + 115200U / 2U) / 115200U);
  driver::reg::write(USART2->CR1, USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);
}

void uartPutChar(char c) {
  while (!driver::reg::read(USART2->SR, USART_SR_TXE)) {
  }
  driver::reg::write(
      USART2->DR,
      static_cast<uint32_t>(static_cast<uint8_t>(c))
  );
}

void uartPutStr(const char *s) {
  while (*s != '\0') {
    uartPutChar(*s++);
  }
}

void busyDelayLoops(uint32_t loops) {
  for (uint32_t i = 0; i < loops; ++i) {
    __asm volatile("nop");
  }
}

}  // namespace

int main() {
  uartInit();
  char buf[96];

  while (true) {
    uartPutStr("\r\n=== I2C1 scan (PB6/PB7 @ 100 kHz) ===\r\n");
    size_t found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
      if (g_i2c1.probe(addr) == driver::Status::Ok) {
        const int len = snprintf(
            buf,
            sizeof(buf),
            "  found device at 7-bit 0x%02X (8-bit write 0x%02X / read "
            "0x%02X)\r\n",
            addr,
            static_cast<unsigned>(addr << 1),
            static_cast<unsigned>((addr << 1) | 1)
        );
        if (len > 0) {
          buf[len] = '\0';
          uartPutStr(buf);
        }
        ++found;
      }
    }
    const int len = snprintf(
        buf,
        sizeof(buf),
        "scan complete: %u device(s)\r\n",
        static_cast<unsigned>(found)
    );
    if (len > 0) {
      buf[len] = '\0';
      uartPutStr(buf);
    }
    busyDelayLoops(20000000U);
  }
}
