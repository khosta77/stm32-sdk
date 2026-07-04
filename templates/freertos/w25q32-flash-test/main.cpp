#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"

import driver.types;
import driver.gpio;
import driver.reg;
import driver.spi;
import driver.uart;
import driver.stm32f4.gpio;
import driver.stm32f4.spi;
import driver.stm32f4.uart;
import sensor.external_flash;
import sensor.w25q32;

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::DataBits;
using driver::gpio;
using driver::GpioConfig;
using driver::OutputSpeed;
using driver::OutputType;
using driver::Parity;
using driver::PinMode;
using driver::PullMode;
using driver::spi;
using driver::SpiDataSize;
using driver::SpiMode;
using driver::StopBits;
using driver::uart;
using driver::stm32f4::GpioPin;
using driver::stm32f4::Spi;
using driver::stm32f4::Uart;

extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN);
  driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_USART2EN | RCC_APB1ENR_SPI2EN);
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
GpioPin g_uartRx{
    *GPIOA,
    gpio({
        .pin = 3,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 7,
    }),
};
GpioPin g_spiSck{
    *GPIOB,
    gpio({
        .pin = 13,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 5,
    }),
};
GpioPin g_spiMiso{
    *GPIOB,
    gpio({
        .pin = 14,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 5,
    }),
};
GpioPin g_spiMosi{
    *GPIOB,
    gpio({
        .pin = 15,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 5,
    }),
};
GpioPin g_flashCs{
    *GPIOB,
    gpio({
        .pin = 12,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
    }),
};

Spi g_spi2{
    *SPI2,
    spi({
        .clockHz = 10000000,
        .mode = SpiMode::Mode0,
        .lsbFirst = false,
        .dataSize = SpiDataSize::Bits8,
    }),
};
Uart<> g_uart2{
    *USART2,
    USART2_IRQn,
    uart({
        .baudrate = 115200,
        .dataBits = DataBits::Eight,
        .stopBits = StopBits::One,
        .parity = Parity::None,
    }),
};
sensor::W25q32 g_flash{
    g_spi2,
    g_flashCs,
    {
        .expectedJedecId = sensor::W25q32Spec::JEDEC_W25Q32JV,
        .busyPollLoops = 5000000U,
    },
};

void writeStr(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') {
    ++n;
  }
  (void) g_uart2.write({reinterpret_cast<const uint8_t *>(s), n});
}

void writePrintf(const char *fmt, auto... args) {
  char buf[128];
  int len = snprintf(buf, sizeof(buf), fmt, args...);
  if (len > 0) {
    (void) g_uart2.write(
        {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
    );
  }
}

void hexDump(const uint8_t *buf, size_t len) {
  char line[80];
  for (size_t i = 0; i < len; i += 16) {
    int p = snprintf(line, sizeof(line), "  %04X:", static_cast<unsigned>(i));
    for (size_t j = 0; j < 16 && (i + j) < len; ++j) {
      p += snprintf(
          line + p,
          sizeof(line) - p,
          " %02X",
          static_cast<unsigned>(buf[i + j])
      );
    }
    snprintf(line + p, sizeof(line) - p, "\r\n");
    writeStr(line);
  }
}

void taskFlashTest(void *) {
  rtos::Task::delay(pdMS_TO_TICKS(200));
  writeStr("\r\n=== W25Q32 flash test ===\r\n");

  auto st = g_flash.init();
  if (st != driver::Status::Ok) {
    writePrintf(
        "init() failed: status=%d, JEDEC ID 0x%06lX (expected 0x%06lX)\r\n",
        static_cast<int>(st),
        static_cast<unsigned long>(g_flash.jedecId()),
        static_cast<unsigned long>(sensor::W25q32Spec::JEDEC_W25Q32JV)
    );
    while (true) {
      rtos::Task::delay(pdMS_TO_TICKS(1000));
    }
  }
  writePrintf(
      "JEDEC ID: 0x%06lX\r\n",
      static_cast<unsigned long>(g_flash.jedecId())
  );
  writePrintf(
      "capacity=%lu, sectorSize=%lu, pageSize=%lu\r\n",
      static_cast<unsigned long>(g_flash.capacity()),
      static_cast<unsigned long>(g_flash.sectorSize()),
      static_cast<unsigned long>(g_flash.pageSize())
  );

  constexpr uint32_t kAddr = 0x000000;
  constexpr size_t kPayloadLen = 64;
  uint8_t txBuf[kPayloadLen];
  for (size_t i = 0; i < kPayloadLen; ++i) {
    txBuf[i] = static_cast<uint8_t>(0xA0 + (i & 0x0F));
  }

  writeStr("erase sector 0x000000...\r\n");
  st = g_flash.eraseSector(kAddr);
  if (st != driver::Status::Ok) {
    writePrintf("eraseSector failed: %d\r\n", static_cast<int>(st));
    return;
  }

  writeStr("page program 64 bytes...\r\n");
  st = g_flash.writePage(kAddr, {txBuf, kPayloadLen});
  if (st != driver::Status::Ok) {
    writePrintf("writePage failed: %d\r\n", static_cast<int>(st));
    return;
  }

  uint8_t rxBuf[kPayloadLen] = {0};
  writeStr("read back 64 bytes:\r\n");
  st = g_flash.read(kAddr, {rxBuf, kPayloadLen});
  if (st != driver::Status::Ok) {
    writePrintf("read failed: %d\r\n", static_cast<int>(st));
    return;
  }
  hexDump(rxBuf, kPayloadLen);

  bool match = true;
  for (size_t i = 0; i < kPayloadLen; ++i) {
    if (rxBuf[i] != txBuf[i]) {
      match = false;
      break;
    }
  }
  writeStr(match ? "VERIFY OK\r\n" : "VERIFY MISMATCH\r\n");

  while (true) {
    rtos::Task::delay(pdMS_TO_TICKS(5000));
    writeStr(".");
  }
}

}  // namespace

extern "C" void USART2_IRQHandler() {
  g_uart2.irqHandler();
}

int main() {
  static rtos::Task t("flash", 512, 1, taskFlashTest);

  rtos::Task::startScheduler();

  while (true) {
  }
}
