module;
#include <cstddef>
#include <cstdint>
#include <span>
#include "cmsis/stm32f4xx.h"
#ifdef STM32_USE_FREERTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif
export module driver.stm32f4.i2c;

import driver.types;
import driver.i2c;
import driver.reg;
import driver.stm32f4.clock;

export namespace driver {
namespace stm32f4 {

class I2c {
private:
  I2C_TypeDef &_periph;
  I2cConfig _cfg;

#ifdef STM32_USE_FREERTOS
  SemaphoreHandle_t _mutex = nullptr;
#endif

  bool waitFlag(volatile uint32_t &reg, uint32_t flag, bool set) const {
    for (uint32_t i = 0, n = getTimeoutLoops(); i < n; ++i) {
      if (set) {
        if (reg & flag) {
          return true;
        }
      } else {
        if (!(reg & flag)) {
          return true;
        }
      }
    }
    return false;
  }

  bool waitBusy() const { return waitFlag(_periph.SR2, I2C_SR2_BUSY, false); }

  void generateStart() const {
    reg::set(_periph.CR1, I2C_CR1_START);
    waitFlag(_periph.SR1, I2C_SR1_SB, true);
  }

  void generateStop() const { reg::set(_periph.CR1, I2C_CR1_STOP); }

  bool sendAddress(uint8_t addr, bool readOp) const {
    _periph.DR = static_cast<uint32_t>((addr << 1) | (readOp ? 1 : 0));
    // Race ADDR (slave ACK'd, success) against AF (slave NACK'd, fail). On
    // NACK the AF bit is set in ~9 SCL cycles, so this lets probe() and
    // any other transaction abort almost immediately for absent addresses
    // instead of burning the full waitFlag timeout.
    for (uint32_t i = 0, n = getTimeoutLoops(); i < n; ++i) {
      const uint32_t sr1 = reg::get(_periph.SR1);
      if (sr1 & I2C_SR1_ADDR) {
        (void) reg::get(_periph.SR2);
        return true;
      }
      if (sr1 & I2C_SR1_AF) {
        reg::clear(_periph.SR1, I2C_SR1_AF);
        generateStop();
        return false;
      }
    }
    return false;
  }

  void busRecovery() {
    reg::set(_periph.CR1, I2C_CR1_SWRST);
    reg::clear(_periph.CR1, I2C_CR1_SWRST);
    reinit();
  }

  void reinit() {
    uint32_t pclk = getApb1Clock();
    uint32_t freqMhz = pclk / 1000000;
    _periph.CR2 = freqMhz & I2C_CR2_FREQ;

    if (_cfg.fastMode || _cfg.clockSpeed > 100000) {
      _periph.CCR = I2C_CCR_FS | (pclk / (_cfg.clockSpeed * 3));
      _periph.TRISE = (freqMhz * 300 / 1000) + 1;
    } else {
      _periph.CCR = pclk / (_cfg.clockSpeed * 2);
      _periph.TRISE = freqMhz + 1;
    }

    reg::write(_periph.CR1, I2C_CR1_PE);
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

  void criticalEnter() {
#ifdef STM32_USE_FREERTOS
    taskENTER_CRITICAL();
#else
    __disable_irq();
#endif
  }

  void criticalExit() {
#ifdef STM32_USE_FREERTOS
    taskEXIT_CRITICAL();
#else
    __enable_irq();
#endif
  }

  Status readBytes(uint8_t addr, std::span<uint8_t> data) {
    const size_t N = data.size();
    if (N == 0) {
      generateStop();
      return Status::Ok;
    }

    if (N == 1) {
      reg::clear(_periph.CR1, I2C_CR1_ACK);
      reg::clear(_periph.CR1, I2C_CR1_POS);
      generateStart();
      _periph.DR = static_cast<uint32_t>((addr << 1) | 1);
      if (!waitFlag(_periph.SR1, I2C_SR1_ADDR, true)) {
        if (reg::read(_periph.SR1, I2C_SR1_AF)) {
          reg::clear(_periph.SR1, I2C_SR1_AF);
        }
        return Status::Nack;
      }
      criticalEnter();
      volatile uint32_t sr2 = _periph.SR2;
      (void) sr2;
      generateStop();
      criticalExit();
      if (!waitFlag(_periph.SR1, I2C_SR1_RXNE, true)) {
        return Status::Timeout;
      }
      data[0] = static_cast<uint8_t>(_periph.DR);
      return Status::Ok;
    }

    if (N == 2) {
      reg::set(_periph.CR1, I2C_CR1_ACK | I2C_CR1_POS);
      generateStart();
      _periph.DR = static_cast<uint32_t>((addr << 1) | 1);
      if (!waitFlag(_periph.SR1, I2C_SR1_ADDR, true)) {
        if (reg::read(_periph.SR1, I2C_SR1_AF)) {
          reg::clear(_periph.SR1, I2C_SR1_AF);
        }
        reg::clear(_periph.CR1, I2C_CR1_POS);
        return Status::Nack;
      }
      criticalEnter();
      volatile uint32_t sr2 = _periph.SR2;
      (void) sr2;
      reg::clear(_periph.CR1, I2C_CR1_ACK);
      criticalExit();
      if (!waitFlag(_periph.SR1, I2C_SR1_BTF, true)) {
        reg::clear(_periph.CR1, I2C_CR1_POS);
        return Status::Timeout;
      }
      criticalEnter();
      generateStop();
      data[0] = static_cast<uint8_t>(_periph.DR);
      data[1] = static_cast<uint8_t>(_periph.DR);
      criticalExit();
      reg::clear(_periph.CR1, I2C_CR1_POS);
      return Status::Ok;
    }

    reg::clear(_periph.CR1, I2C_CR1_POS);
    reg::set(_periph.CR1, I2C_CR1_ACK);
    generateStart();
    if (!sendAddress(addr, true)) {
      return Status::Nack;
    }
    size_t i = 0;
    while (N - i > 3) {
      if (!waitFlag(_periph.SR1, I2C_SR1_RXNE, true)) {
        return Status::Timeout;
      }
      data[i++] = static_cast<uint8_t>(_periph.DR);
    }
    if (!waitFlag(_periph.SR1, I2C_SR1_BTF, true)) {
      return Status::Timeout;
    }
    criticalEnter();
    reg::clear(_periph.CR1, I2C_CR1_ACK);
    data[i++] = static_cast<uint8_t>(_periph.DR);
    criticalExit();
    if (!waitFlag(_periph.SR1, I2C_SR1_BTF, true)) {
      return Status::Timeout;
    }
    criticalEnter();
    generateStop();
    data[i++] = static_cast<uint8_t>(_periph.DR);
    criticalExit();
    if (!waitFlag(_periph.SR1, I2C_SR1_RXNE, true)) {
      return Status::Timeout;
    }
    data[i] = static_cast<uint8_t>(_periph.DR);
    return Status::Ok;
  }

public:
  I2c(I2C_TypeDef &periph, const I2cConfig &cfg) : _periph(periph), _cfg(cfg) {
    reg::write(_periph.CR1, I2C_CR1_SWRST);
    reg::write(_periph.CR1, 0);
    reinit();

#ifdef STM32_USE_FREERTOS
    _mutex = xSemaphoreCreateMutex();
    configASSERT(_mutex);
#endif
  }

  ~I2c() {
    reg::write(_periph.CR1, 0);

#ifdef STM32_USE_FREERTOS
    if (_mutex) {
      vSemaphoreDelete(_mutex);
    }
#endif
  }

  I2c(const I2c &) = delete;
  I2c &operator=(const I2c &) = delete;

  [[nodiscard]] Status write(uint8_t addr, std::span<const uint8_t> data) {
    lockBus();

    Status result = Status::BusError;
    if (waitBusy()) {
      generateStart();
      if (sendAddress(addr, false)) {
        result = Status::Ok;
        for (auto byte : data) {
          if (!waitFlag(_periph.SR1, I2C_SR1_TXE, true)) {
            result = Status::Timeout;
            break;
          }
          _periph.DR = byte;
        }
        if (result == Status::Ok) {
          waitFlag(_periph.SR1, I2C_SR1_BTF, true);
        }
        generateStop();
      } else {
        result = Status::Nack;
      }
    }

    if (result != Status::Ok) {
      busRecovery();
    }
    unlockBus();
    return result;
  }

  [[nodiscard]] Status read(uint8_t addr, std::span<uint8_t> data) {
    lockBus();

    Status result = Status::BusError;
    if (waitBusy()) {
      result = readBytes(addr, data);
    }

    if (result != Status::Ok) {
      generateStop();
      busRecovery();
    }
    unlockBus();
    return result;
  }

  [[nodiscard]] Status
  writeReg(uint8_t addr, uint8_t reg, std::span<const uint8_t> data) {
    lockBus();

    Status result = Status::BusError;
    if (waitBusy()) {
      generateStart();
      if (sendAddress(addr, false)) {
        if (waitFlag(_periph.SR1, I2C_SR1_TXE, true)) {
          _periph.DR = reg;
          result = Status::Ok;
          for (auto byte : data) {
            if (!waitFlag(_periph.SR1, I2C_SR1_TXE, true)) {
              result = Status::Timeout;
              break;
            }
            _periph.DR = byte;
          }
          if (result == Status::Ok) {
            waitFlag(_periph.SR1, I2C_SR1_BTF, true);
          }
        } else {
          result = Status::Timeout;
        }
        generateStop();
      } else {
        result = Status::Nack;
      }
    }

    if (result != Status::Ok) {
      busRecovery();
    }
    unlockBus();
    return result;
  }

  [[nodiscard]] Status
  readReg(uint8_t addr, uint8_t reg, std::span<uint8_t> data) {
    lockBus();

    Status result = Status::BusError;
    if (waitBusy()) {
      generateStart();
      if (sendAddress(addr, false)) {
        if (waitFlag(_periph.SR1, I2C_SR1_TXE, true)) {
          _periph.DR = reg;
          if (waitFlag(_periph.SR1, I2C_SR1_BTF, true)) {
            result = readBytes(addr, data);
          } else {
            result = Status::Timeout;
          }
        } else {
          result = Status::Timeout;
        }
      } else {
        result = Status::Nack;
      }
    }

    if (result != Status::Ok) {
      generateStop();
      busRecovery();
    }
    unlockBus();
    return result;
  }

  [[nodiscard]] Status probe(uint8_t addr) {
    lockBus();

    Status result = Status::Nack;
    if (waitBusy()) {
      generateStart();
      if (sendAddress(addr, false)) {
        result = Status::Ok;
      }
      generateStop();
    } else {
      result = Status::Busy;
    }

    unlockBus();
    return result;
  }
};

static_assert(II2c<I2c>, "I2c must model driver::II2c");

}  // namespace stm32f4
}  // namespace driver
