module;
#include <cstddef>
#include <cstdint>
#include <span>
export module testing.mock;

import driver.types;
import driver.i2c;
import driver.spi;
import driver.uart;
import driver.gpio;
import driver.flash;

// Reusable, concept-satisfying mock buses for host unit tests (issue #34).
// Each mock is a plain struct that models the matching driver concept
// (driver::II2c / ISpi / IUart / IGpioPin / IFlash), so a driver or sensor
// templated on its bus can be exercised on the host with no hardware. They are
// programmable: `loadResponse`/`loadRx` script the bytes the bus hands back on
// read, and `written()`/`transmitted()` expose what the code under test sent,
// so a test asserts behaviour instead of only "returns Ok". No heap, no
// exceptions -- the same struct also compiles on device. All loops are indexed
// (never range-for over std::span across a module boundary; GCC 15 gotcha).

export namespace testing {

using driver::Status;

// Fixed capacity for the scripted-response and capture buffers. Large enough
// for register-level sensor exchanges; overflow is silently clamped.
inline constexpr size_t kMockCapacity = 256;

namespace detail {

inline size_t
appendBytes(uint8_t *dst, size_t len, std::span<const uint8_t> src) {
  size_t n = 0;
  while (n < src.size() && len + n < kMockCapacity) {
    dst[len + n] = src[n];
    ++n;
  }
  return n;
}

inline void drainInto(
    const uint8_t *src,
    size_t total,
    size_t &pos,
    std::span<uint8_t> out
) {
  for (size_t i = 0; i < out.size(); ++i) {
    out[i] = (pos < total) ? src[pos++] : uint8_t{0};
  }
}

}  // namespace detail

// Models driver::II2c. read()/readReg() return scripted bytes in order;
// write()/writeReg() append to the capture buffer (writeReg records the
// register byte first). `nextStatus` lets a test inject bus errors.
struct MockI2c {
  uint8_t responses[kMockCapacity]{};
  size_t responseLen{0};
  size_t responsePos{0};
  uint8_t captured[kMockCapacity]{};
  size_t capturedLen{0};
  uint8_t lastAddr{0};
  uint8_t lastReg{0};
  uint32_t probeCount{0};
  Status nextStatus{Status::Ok};

  void loadResponse(std::span<const uint8_t> bytes) {
    responseLen = 0;
    responsePos = 0;
    responseLen = detail::appendBytes(responses, 0, bytes);
  }
  [[nodiscard]] std::span<const uint8_t> written() const {
    return std::span<const uint8_t>{captured, capturedLen};
  }

  Status write(uint8_t addr, std::span<const uint8_t> data) {
    lastAddr = addr;
    capturedLen += detail::appendBytes(captured, capturedLen, data);
    return nextStatus;
  }
  Status read(uint8_t addr, std::span<uint8_t> out) {
    lastAddr = addr;
    detail::drainInto(responses, responseLen, responsePos, out);
    return nextStatus;
  }
  Status writeReg(uint8_t addr, uint8_t reg, std::span<const uint8_t> data) {
    lastAddr = addr;
    lastReg = reg;
    if (capturedLen < kMockCapacity) {
      captured[capturedLen++] = reg;
    }
    capturedLen += detail::appendBytes(captured, capturedLen, data);
    return nextStatus;
  }
  Status readReg(uint8_t addr, uint8_t reg, std::span<uint8_t> out) {
    lastAddr = addr;
    lastReg = reg;
    detail::drainInto(responses, responseLen, responsePos, out);
    return nextStatus;
  }
  Status probe(uint8_t addr) {
    lastAddr = addr;
    ++probeCount;
    return nextStatus;
  }
};
static_assert(driver::II2c<MockI2c>, "MockI2c must model driver::II2c");

// Models driver::ISpi. transfer() captures tx and fills rx from the scripted
// response; write()/read() are the half-duplex variants.
struct MockSpi {
  uint8_t responses[kMockCapacity]{};
  size_t responseLen{0};
  size_t responsePos{0};
  uint8_t captured[kMockCapacity]{};
  size_t capturedLen{0};
  Status nextStatus{Status::Ok};

  void loadResponse(std::span<const uint8_t> bytes) {
    responseLen = detail::appendBytes(responses, 0, bytes);
    responsePos = 0;
  }
  [[nodiscard]] std::span<const uint8_t> written() const {
    return std::span<const uint8_t>{captured, capturedLen};
  }

  Status transfer(std::span<const uint8_t> tx, std::span<uint8_t> rx) {
    capturedLen += detail::appendBytes(captured, capturedLen, tx);
    detail::drainInto(responses, responseLen, responsePos, rx);
    return nextStatus;
  }
  Status write(std::span<const uint8_t> tx) {
    capturedLen += detail::appendBytes(captured, capturedLen, tx);
    return nextStatus;
  }
  Status read(std::span<uint8_t> rx) {
    detail::drainInto(responses, responseLen, responsePos, rx);
    return nextStatus;
  }
};
static_assert(driver::ISpi<MockSpi>, "MockSpi must model driver::ISpi");

// Models driver::IUart. read() drains the scripted RX buffer; write() records
// into the TX buffer. Returns byte counts like the real UART.
struct MockUart {
  uint8_t rx[kMockCapacity]{};
  size_t rxLen{0};
  size_t rxPos{0};
  uint8_t tx[kMockCapacity]{};
  size_t txLen{0};

  void loadRx(std::span<const uint8_t> bytes) {
    rxLen = detail::appendBytes(rx, 0, bytes);
    rxPos = 0;
  }
  [[nodiscard]] std::span<const uint8_t> transmitted() const {
    return std::span<const uint8_t>{tx, txLen};
  }

  size_t write(std::span<const uint8_t> data) {
    size_t n = detail::appendBytes(tx, txLen, data);
    txLen += n;
    return n;
  }
  size_t read(std::span<uint8_t> out) {
    size_t start = rxPos;
    detail::drainInto(rx, rxLen, rxPos, out);
    size_t served = rxPos - start;
    return served < out.size() ? served : out.size();
  }
  size_t writeNonBlocking(std::span<const uint8_t> data) { return write(data); }
  size_t readNonBlocking(std::span<uint8_t> out) { return read(out); }
  [[nodiscard]] size_t rxAvailable() const { return rxLen - rxPos; }
  [[nodiscard]] size_t txFree() const { return kMockCapacity - txLen; }
  void irqHandler() {}
};
static_assert(driver::IUart<MockUart>, "MockUart must model driver::IUart");

// Models driver::IGpioPin. Tracks the level and counts each operation so a test
// can assert a pin was driven as expected. read() reports the level as
// Status::Ok (high) / Status::None (low).
struct MockGpioPin {
  bool high{false};
  uint32_t setCount{0};
  uint32_t resetCount{0};
  uint32_t toggleCount{0};

  void set() {
    high = true;
    ++setCount;
  }
  void reset() {
    high = false;
    ++resetCount;
  }
  void toggle() {
    high = !high;
    ++toggleCount;
  }
  [[nodiscard]] Status read() const { return high ? Status::Ok : Status::None; }
};
static_assert(
    driver::IGpioPin<MockGpioPin>,
    "MockGpioPin must model driver::IGpioPin"
);

// Models driver::IFlash over a small in-memory array. eraseSector() sets the
// sector to 0xFF (NOR-erase semantics); read()/write() copy at byte offsets.
struct MockFlash {
  static constexpr size_t kSectorSize = 64;
  static constexpr uint8_t kSectorCount = kMockCapacity / kSectorSize;
  uint8_t mem[kMockCapacity]{};
  Status nextStatus{Status::Ok};

  Status read(uint32_t addr, std::span<uint8_t> out) {
    for (size_t i = 0; i < out.size(); ++i) {
      size_t idx = static_cast<size_t>(addr) + i;
      out[i] = (idx < kMockCapacity) ? mem[idx] : uint8_t{0xFF};
    }
    return nextStatus;
  }
  Status write(uint32_t addr, std::span<const uint8_t> in) {
    for (size_t i = 0; i < in.size(); ++i) {
      size_t idx = static_cast<size_t>(addr) + i;
      if (idx < kMockCapacity) {
        mem[idx] = in[i];
      }
    }
    return nextStatus;
  }
  Status eraseSector(uint8_t sector) {
    size_t base = static_cast<size_t>(sector) * kSectorSize;
    for (size_t i = 0; i < kSectorSize && base + i < kMockCapacity; ++i) {
      mem[base + i] = 0xFF;
    }
    return nextStatus;
  }
  [[nodiscard]] size_t sectorSize(uint8_t) const { return kSectorSize; }
  [[nodiscard]] uint8_t sectorCount() const { return kSectorCount; }
};
static_assert(driver::IFlash<MockFlash>, "MockFlash must model driver::IFlash");

}  // namespace testing
