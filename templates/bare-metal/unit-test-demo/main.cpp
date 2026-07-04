#include <cstddef>
#include <cstdint>
#include "cmsis/stm32f4xx.h"
#include "testing/unit_test.hpp"

import driver.types;
import driver.gpio;
import driver.reg;
import driver.stm32f4.clock;
import driver.stm32f4.gpio;

using driver::gpio;
using driver::OutputSpeed;
using driver::OutputType;
using driver::PinMode;
using driver::PullMode;
using driver::Result;
using driver::Status;
using driver::stm32f4::GpioPin;

extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN);
  driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_USART2EN);
  __DSB();
}

namespace {

GpioPin g_uartTx{
    *GPIOA,
    gpio({
        .pin = 2,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 7,
    }),
};

void uartInit() {
  const uint32_t pclk = driver::stm32f4::getApb1Clock();
  driver::reg::write(USART2->BRR, (pclk + 115200U / 2U) / 115200U);
  driver::reg::write(USART2->CR1, USART_CR1_UE | USART_CR1_TE);
}

void uartPutChar(char c) {
  while (!driver::reg::read(USART2->SR, USART_SR_TXE)) {
  }
  driver::reg::write(
      USART2->DR,
      static_cast<uint32_t>(static_cast<uint8_t>(c))
  );
}

// Signature matches testing::Writer (void(*)(const char*)) so it can be handed
// straight to the runner. On the host, swap this for one that writes to stdout
// and the tests below compile and run unchanged.
void uartPutStr(const char *s) {
  while (*s != '\0') {
    uartPutChar(*s++);
  }
}

// Tests exercise driver::Result<T> -- pure, hardware-independent logic, so the
// exact same bodies run on the host too. ASSERT_ stops the test on failure;
// EXPECT_ records the failure and keeps checking.
TEST(result_ok_carries_value) {
  Result<int> r{42};
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), 42);
  EXPECT_EQ(r.valueOr(0), 42);
}

TEST(result_error_reports_status) {
  Result<int> r{Status::Timeout};
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status(), Status::Timeout);
  EXPECT_EQ(r.valueOr(7), 7);
}

TEST(result_ok_status_normalizes) {
  // A Result built from Status::Ok means "no value", not success: the status
  // is normalized to None so an Ok-as-error can never masquerade as a value.
  Result<int> r{Status::Ok};
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status(), Status::None);
}

}  // namespace

int main() {
  uartInit();

  testing::TestRunner runner{uartPutStr};
  uartPutStr("\r\n=== unit-test-demo ===\r\n");

  RUN_TEST(runner, result_ok_carries_value);
  RUN_TEST(runner, result_error_reports_status);
  RUN_TEST(runner, result_ok_status_normalizes);

  (void) runner.summary();

  while (true) {
  }
}
