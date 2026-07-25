module;
#include <cstddef>
#include <cstdint>
#include <span>
#include "cmsis/stm32f4xx.h"
#ifdef STM32_USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif
export module driver.stm32f4.spi;

import driver.types;
import driver.spi;
import driver.reg;
import driver.stm32f4.clock;

export namespace driver {
namespace stm32f4 {

// SPI DMA stream IDs are reserved in driver.stm32f4.dma (dmaMap::spi1_tx etc.).
// Async transfer via DMA will land in a follow-up PR.

class Spi {
private:
  SPI_TypeDef &_periph;
  SpiConfig _cfg;

#ifdef STM32_USE_FREERTOS
  SemaphoreHandle_t _mutex = nullptr;
#endif

  bool waitTxe() const {
    for (uint32_t i = 0, n = getTimeoutLoops(); i < n; ++i) {
      if (reg::read(_periph.SR, SPI_SR_TXE)) {
        return true;
      }
    }
    return false;
  }

  bool waitRxne() const {
    for (uint32_t i = 0, n = getTimeoutLoops(); i < n; ++i) {
      if (reg::read(_periph.SR, SPI_SR_RXNE)) {
        return true;
      }
    }
    return false;
  }

  bool waitNotBusy() const {
    for (uint32_t i = 0, n = getTimeoutLoops(); i < n; ++i) {
      if (!reg::read(_periph.SR, SPI_SR_BSY)) {
        return true;
      }
    }
    return false;
  }

  void lockBus() {
#ifdef STM32_USE_FREERTOS
    xSemaphoreTake(_mutex, portMAX_DELAY);
#endif
  }

  void unlockBus() {
#ifdef STM32_USE_FREERTOS
    xSemaphoreGive(_mutex);
#endif
  }

public:
  Spi(SPI_TypeDef &periph, const SpiConfig &cfg) : _periph(periph), _cfg(cfg) {
    reg::write(_periph.CR1, 0);

    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM;

    // SPI1, SPI4, SPI5, SPI6 sit on APB2; SPI2 and SPI3 sit on APB1.
    uint32_t pclk;
    if (&_periph == SPI2 || &_periph == SPI3) {
      pclk = getApb1Clock();
    } else {
      pclk = getApb2Clock();
    }
    uint32_t div = pclk / _cfg.clockHz;
    uint32_t br = 0;
    while ((1U << (br + 1)) < div && br < 7) {
      ++br;
    }
    cr1 |= (br << SPI_CR1_BR_Pos);

    const uint8_t mode = static_cast<uint8_t>(_cfg.mode);
    if (mode & 0x01) {
      cr1 |= SPI_CR1_CPHA;
    }
    if (mode & 0x02) {
      cr1 |= SPI_CR1_CPOL;
    }
    if (_cfg.lsbFirst) {
      cr1 |= SPI_CR1_LSBFIRST;
    }
    // Data size is fixed at 8 bits: SpiConfig rejects Bits16 at compile time
    // (the transfer path drives DR one byte at a time). DFF is left cleared.

    reg::write(_periph.CR1, cr1);
    reg::set(_periph.CR1, SPI_CR1_SPE);

#ifdef STM32_USE_FREERTOS
    _mutex = xSemaphoreCreateMutex();
    configASSERT(_mutex);
#endif
  }

  ~Spi() {
    waitNotBusy();
    reg::write(_periph.CR1, 0);

#ifdef STM32_USE_FREERTOS
    if (_mutex) {
      vSemaphoreDelete(_mutex);
    }
#endif
  }

  Spi(const Spi &) = delete;
  Spi &operator=(const Spi &) = delete;

  [[nodiscard]] Status
  transfer(std::span<const uint8_t> txData, std::span<uint8_t> rxData) {
    lockBus();

    Status result = Status::Ok;
    for (size_t i = 0, I = txData.size(); i < I; ++i) {
      if (!waitTxe()) {
        result = Status::Timeout;
        break;
      }
      _periph.DR = txData[i];
      if (!waitRxne()) {
        result = Status::Timeout;
        break;
      }
      uint8_t rx = static_cast<uint8_t>(_periph.DR);
      if (i < rxData.size()) {
        rxData[i] = rx;
      }
    }
    waitNotBusy();

    unlockBus();
    return result;
  }

  [[nodiscard]] Status write(std::span<const uint8_t> data) {
    lockBus();

    Status result = Status::Ok;
    for (size_t i = 0, I = data.size(); i < I; ++i) {
      if (!waitTxe()) {
        result = Status::Timeout;
        break;
      }
      _periph.DR = data[i];
      if (!waitRxne()) {
        result = Status::Timeout;
        break;
      }
      volatile uint8_t discard = static_cast<uint8_t>(_periph.DR);
      (void) discard;
    }
    waitNotBusy();

    unlockBus();
    return result;
  }

  [[nodiscard]] Status read(std::span<uint8_t> data) {
    lockBus();

    Status result = Status::Ok;
    for (size_t i = 0, I = data.size(); i < I; ++i) {
      if (!waitTxe()) {
        result = Status::Timeout;
        break;
      }
      _periph.DR = 0xFF;
      if (!waitRxne()) {
        result = Status::Timeout;
        break;
      }
      data[i] = static_cast<uint8_t>(_periph.DR);
    }
    waitNotBusy();

    unlockBus();
    return result;
  }
};

static_assert(ISpi<Spi>, "Spi must model driver::ISpi");

}  // namespace stm32f4
}  // namespace driver
