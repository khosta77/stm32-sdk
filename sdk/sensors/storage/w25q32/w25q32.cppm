module;
#include <cstddef>
#include <cstdint>
#include <span>
export module sensor.w25q32;

import driver.types;
import driver.spi;
import driver.gpio;
import sensor.external_flash;

export namespace sensor {

// Device geometry / identity — non-template so user code can refer to it as
// sensor::W25q32Spec::SECTOR_SIZE without naming the bus/CS template arguments.
struct W25q32Spec {
  static constexpr uint32_t CAPACITY = 4U * 1024U * 1024U;  // 32 Mbit = 4 MiB
  static constexpr uint32_t SECTOR_SIZE = 4096U;
  static constexpr uint32_t PAGE_SIZE = 256U;
  static constexpr uint32_t JEDEC_W25Q32JV = 0xEF4016U;
};

template <driver::ISpi SpiDriver, driver::IGpioPin GpioDriver>
class W25q32 {
public:
  static constexpr uint32_t CAPACITY = W25q32Spec::CAPACITY;
  static constexpr uint32_t SECTOR_SIZE = W25q32Spec::SECTOR_SIZE;
  static constexpr uint32_t PAGE_SIZE = W25q32Spec::PAGE_SIZE;
  static constexpr uint32_t JEDEC_W25Q32JV = W25q32Spec::JEDEC_W25Q32JV;

  struct Config {
    uint32_t expectedJedecId;  // typically JEDEC_W25Q32JV
    uint32_t busyPollLoops;    // safety bound for waitNotBusy()
  };

private:
  enum Cmd : uint8_t {
    WRITE_ENABLE = 0x06,
    WRITE_DISABLE = 0x04,
    READ_STATUS_1 = 0x05,
    READ_DATA = 0x03,
    PAGE_PROGRAM = 0x02,
    SECTOR_ERASE_4K = 0x20,
    BLOCK_ERASE_64K = 0xD8,
    CHIP_ERASE = 0xC7,
    JEDEC_ID = 0x9F,
  };

  SpiDriver &_spi;
  GpioDriver &_cs;
  Config _cfg;
  uint32_t _jedec = 0;

public:
  W25q32(SpiDriver &spi, GpioDriver &cs, const Config &cfg)
      : _spi(spi), _cs(cs), _cfg(cfg) {
    _cs.set();  // CS is active-low; deselect at construction
  }

  W25q32(const W25q32 &) = delete;
  W25q32 &operator=(const W25q32 &) = delete;

  [[nodiscard]] driver::Status init() {
    const uint8_t cmd = JEDEC_ID;
    uint8_t id[3] = {0, 0, 0};

    _cs.reset();
    auto st = _spi.write({&cmd, 1});
    if (st == driver::Status::Ok) {
      st = _spi.read({id, 3});
    }
    _cs.set();

    if (st != driver::Status::Ok) {
      return st;
    }
    _jedec = (static_cast<uint32_t>(id[0]) << 16) |
             (static_cast<uint32_t>(id[1]) << 8) | static_cast<uint32_t>(id[2]);
    if (_jedec != _cfg.expectedJedecId) {
      return driver::Status::HardwareError;
    }
    return driver::Status::Ok;
  }

  [[nodiscard]] driver::Status read(uint32_t addr, std::span<uint8_t> data) {
    const uint8_t header[4] = {
        READ_DATA,
        static_cast<uint8_t>((addr >> 16) & 0xFFU),
        static_cast<uint8_t>((addr >> 8) & 0xFFU),
        static_cast<uint8_t>(addr & 0xFFU),
    };
    _cs.reset();
    auto st = _spi.write({header, 4});
    if (st == driver::Status::Ok && !data.empty()) {
      st = _spi.read(data);
    }
    _cs.set();
    return st;
  }

  [[nodiscard]] driver::Status
  writePage(uint32_t addr, std::span<const uint8_t> data) {
    if (data.empty()) {
      return driver::Status::Ok;
    }
    if (data.size() > PAGE_SIZE) {
      return driver::Status::InvalidArg;
    }
    // Programming may not cross a page boundary.
    if ((addr & (PAGE_SIZE - 1)) + data.size() > PAGE_SIZE) {
      return driver::Status::InvalidArg;
    }

    auto st = sendSimpleCmd(WRITE_ENABLE);
    if (st != driver::Status::Ok) {
      return st;
    }

    const uint8_t header[4] = {
        PAGE_PROGRAM,
        static_cast<uint8_t>((addr >> 16) & 0xFFU),
        static_cast<uint8_t>((addr >> 8) & 0xFFU),
        static_cast<uint8_t>(addr & 0xFFU),
    };
    _cs.reset();
    st = _spi.write({header, 4});
    if (st == driver::Status::Ok) {
      st = _spi.write(data);
    }
    _cs.set();

    if (st != driver::Status::Ok) {
      return st;
    }
    return waitNotBusy();
  }

  [[nodiscard]] driver::Status eraseSector(uint32_t addr) {
    auto st = sendSimpleCmd(WRITE_ENABLE);
    if (st != driver::Status::Ok) {
      return st;
    }

    const uint8_t cmd[4] = {
        SECTOR_ERASE_4K,
        static_cast<uint8_t>((addr >> 16) & 0xFFU),
        static_cast<uint8_t>((addr >> 8) & 0xFFU),
        static_cast<uint8_t>(addr & 0xFFU),
    };
    _cs.reset();
    st = _spi.write({cmd, 4});
    _cs.set();

    if (st != driver::Status::Ok) {
      return st;
    }
    return waitNotBusy();
  }

  [[nodiscard]] driver::Status chipErase() {
    auto st = sendSimpleCmd(WRITE_ENABLE);
    if (st != driver::Status::Ok) {
      return st;
    }
    st = sendSimpleCmd(CHIP_ERASE);
    if (st != driver::Status::Ok) {
      return st;
    }
    return waitNotBusy();
  }

  [[nodiscard]] uint32_t capacity() const { return CAPACITY; }
  [[nodiscard]] uint32_t sectorSize() const { return SECTOR_SIZE; }
  [[nodiscard]] uint32_t pageSize() const { return PAGE_SIZE; }
  [[nodiscard]] uint32_t jedecId() const { return _jedec; }

private:
  driver::Status sendSimpleCmd(uint8_t cmd) {
    _cs.reset();
    auto st = _spi.write({&cmd, 1});
    _cs.set();
    return st;
  }

  driver::Status readStatus(uint8_t &out) {
    const uint8_t cmd = READ_STATUS_1;
    _cs.reset();
    auto st = _spi.write({&cmd, 1});
    if (st == driver::Status::Ok) {
      st = _spi.read({&out, 1});
    }
    _cs.set();
    return st;
  }

  driver::Status waitNotBusy() {
    for (uint32_t i = 0; i < _cfg.busyPollLoops; ++i) {
      uint8_t status = 0xFF;
      const auto st = readStatus(status);
      if (st != driver::Status::Ok) {
        return st;
      }
      if ((status & 0x01U) == 0) {
        return driver::Status::Ok;
      }
    }
    return driver::Status::Timeout;
  }
};

}  // namespace sensor

namespace sensor::detail {

// Zero-infra compile-time check (not exported): W25q32 on a trivial ISpi bus
// and IGpioPin CS models sensor::IExternalFlash.
struct MockSpi {
  driver::Status transfer(std::span<const uint8_t>, std::span<uint8_t>) {
    return driver::Status::Ok;
  }
  driver::Status write(std::span<const uint8_t>) { return driver::Status::Ok; }
  driver::Status read(std::span<uint8_t>) { return driver::Status::Ok; }
};
struct MockCs {
  void set() {}
  void reset() {}
  void toggle() {}
  driver::Status read() { return driver::Status::None; }
};
static_assert(
    IExternalFlash<W25q32<MockSpi, MockCs>>,
    "W25q32 must model sensor::IExternalFlash"
);

}  // namespace sensor::detail
