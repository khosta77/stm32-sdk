module;
#include <cstddef>
#include <cstdint>
#include <span>
#include "cmsis/stm32f4xx.h"
export module driver.stm32f4.crc;

import driver.crc;
import driver.soft_crc;
import driver.reg;

export namespace driver {
namespace stm32f4 {

// Hardware CRC unit. The F4 block is the most rigid of the ST line-up: the
// polynomial (0x04C11DB7), the seed (0xFFFFFFFF) and the width are fixed in
// silicon -- CRC_CR carries a single RESET bit, and CRC_DR accepts 32-bit
// words only (byte-wide feeding arrived with F7). Two consequences shape this
// driver:
//
//  * The unit computes MSB-first with no reflection and no final XOR, so it
//    does not natively produce CRC-32/IEEE. Reflecting every input word and
//    the final result with __RBIT converts between the two domains: after
//    RESET, CRC_DR reads 0xFFFFFFFF, whose reflection is exactly
//    Crc32Spec::INIT, and from there __RBIT(CRC_DR) is the running reflected
//    accumulator that softCrcStep() understands.
//  * A buffer whose length is not a multiple of four cannot be fed as-is, so
//    trailing bytes are buffered and folded in by software on read. Chunks of
//    any size may therefore be streamed in any order.
//
// There is exactly one CRC unit per chip and it holds global state: an
// interleaved second user would corrupt both results. Own a single instance
// and serialise access externally, or use driver::SoftCrc, which is
// reentrant.
class Crc {
public:
  explicit Crc(CRC_TypeDef &periph) : _periph(periph) {
    reg::set(RCC->AHB1ENR, RCC_AHB1ENR_CRCEN);
    __DSB();
    reset();
  }

  Crc(const Crc &) = delete;
  Crc &operator=(const Crc &) = delete;

  void reset() {
    reg::set(_periph.CR, CRC_CR_RESET);
    _tailLen = 0;
  }

  void update(std::span<const uint8_t> data) {
    const size_t n = data.size();
    size_t i = 0;

    // Complete the partial word a previous chunk left behind.
    while (_tailLen != 0 && i < n) {
      _tail[_tailLen++] = data[i++];
      if (_tailLen == 4) {
        feedWord(_tail[0], _tail[1], _tail[2], _tail[3]);
        _tailLen = 0;
      }
    }

    // Whole words go straight to the peripheral. Indexed loop, and the bytes
    // are assembled by hand rather than cast through a uint32_t pointer: the
    // incoming span carries no alignment guarantee.
    for (; (n - i) >= 4; i += 4) {
      feedWord(data[i], data[i + 1], data[i + 2], data[i + 3]);
    }

    // 1-3 bytes the hardware cannot take; software folds them in on read.
    for (; i < n; ++i) {
      _tail[_tailLen++] = data[i];
    }
  }

  // Pure read: neither the peripheral nor the tail buffer is disturbed, so
  // this may be called repeatedly and mid-stream.
  [[nodiscard]] uint32_t value() const {
    uint32_t state = __RBIT(reg::get(_periph.DR));
    for (uint8_t i = 0; i < _tailLen; ++i) {
      state = softCrcStep(state, _tail[i]);
    }
    return state ^ Crc32Spec::XOR_OUT;
  }

  [[nodiscard]] uint32_t compute(std::span<const uint8_t> data) {
    reset();
    update(data);
    return value();
  }

private:
  void feedWord(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    const uint32_t word =
        static_cast<uint32_t>(b0) | (static_cast<uint32_t>(b1) << 8) |
        (static_cast<uint32_t>(b2) << 16) | (static_cast<uint32_t>(b3) << 24);
    reg::write(_periph.DR, __RBIT(word));
  }

  CRC_TypeDef &_periph;
  uint8_t _tail[4] = {0, 0, 0, 0};
  uint8_t _tailLen = 0;
};

static_assert(ICrc<Crc>, "Crc must model driver::ICrc");

}  // namespace stm32f4
}  // namespace driver
