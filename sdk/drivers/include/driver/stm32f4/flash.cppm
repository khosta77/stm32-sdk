module;
#include <cstddef>
#include <cstdint>
#include <span>
#include "cmsis/stm32f4xx.h"
export module driver.stm32f4.flash;

import driver.types;
import driver.flash;
import driver.reg;
import driver.stm32f4.clock;

export namespace driver {
namespace stm32f4 {

class InternalFlash {
  static constexpr uint32_t FLASH_KEY1 = 0x45670123;
  static constexpr uint32_t FLASH_KEY2 = 0xCDEF89AB;
  static constexpr uint32_t ERROR_MASK =
      FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR;

  uint8_t _numSectors;

  void unlock() {
    if (reg::read(FLASH->CR, FLASH_CR_LOCK)) {
      reg::write(FLASH->KEYR, FLASH_KEY1);
      reg::write(FLASH->KEYR, FLASH_KEY2);
    }
  }

  void lock() { reg::set(FLASH->CR, FLASH_CR_LOCK); }

  Status waitComplete() const {
    for (uint32_t i = 0, n = getTimeoutLoops(); i < n; ++i) {
      if (!reg::read(FLASH->SR, FLASH_SR_BSY)) {
        return reg::read(FLASH->SR, ERROR_MASK) ? Status::HardwareError
                                                : Status::Ok;
      }
    }
    return Status::Timeout;
  }

  void clearErrors() { reg::write(FLASH->SR, ERROR_MASK | FLASH_SR_EOP); }

  template <typename T>
  static const T *memoryAt(uint32_t addr) {
    return reinterpret_cast<const T *>(addr);
  }

  template <typename T>
  static volatile T *volatileAt(uint32_t addr) {
    return reinterpret_cast<volatile T *>(addr);
  }

public:
  explicit InternalFlash(uint8_t numSectors = 12) : _numSectors(numSectors) {}

  [[nodiscard]] Status read(uint32_t addr, std::span<uint8_t> data) {
    const auto *src = memoryAt<uint8_t>(addr);
    for (size_t i = 0, I = data.size(); i < I; ++i) {
      data[i] = src[i];
    }
    return Status::Ok;
  }

  [[nodiscard]] Status write(uint32_t addr, std::span<const uint8_t> data) {
    unlock();
    clearErrors();

    reg::clear(FLASH->CR, FLASH_CR_PSIZE_Msk);
    reg::set(FLASH->CR, FLASH_CR_PG);

    auto *dst = volatileAt<uint8_t>(addr);
    for (size_t i = 0, I = data.size(); i < I; ++i) {
      dst[i] = data[i];
      Status st = waitComplete();
      if (st != Status::Ok) {
        reg::clear(FLASH->CR, FLASH_CR_PG);
        lock();
        return st;
      }
    }

    reg::clear(FLASH->CR, FLASH_CR_PG);
    lock();
    return Status::Ok;
  }

  [[nodiscard]] Status eraseSector(uint8_t sector) {
    unlock();
    clearErrors();

    reg::clear(FLASH->CR, FLASH_CR_PSIZE_Msk | FLASH_CR_SNB_Msk);
    reg::set(
        FLASH->CR,
        FLASH_CR_SER | (static_cast<uint32_t>(sector) << FLASH_CR_SNB_Pos) |
            (2U << FLASH_CR_PSIZE_Pos)
    );
    reg::set(FLASH->CR, FLASH_CR_STRT);

    Status st = waitComplete();

    reg::clear(FLASH->CR, FLASH_CR_SER | FLASH_CR_SNB_Msk);
    lock();
    return st;
  }

  [[nodiscard]] size_t sectorSize(uint8_t sector) const {
    if (sector < 4) {
      return 16 * 1024;
    }
    if (sector == 4) {
      return 64 * 1024;
    }
    return 128 * 1024;
  }

  [[nodiscard]] uint8_t sectorCount() const { return _numSectors; }
};

static_assert(IFlash<InternalFlash>, "InternalFlash must model driver::IFlash");

}  // namespace stm32f4
}  // namespace driver
