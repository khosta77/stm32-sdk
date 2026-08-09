module;
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.soft_crc;

import driver.crc;

export namespace driver {

// One reflected CRC-32 step over the running state (the accumulator *before*
// XOR_OUT is applied). Exported because the F4 hardware driver reuses it for
// the trailing 1-3 bytes its 32-bit-only CRC->DR cannot accept -- the
// polynomial lives in exactly one place.
constexpr uint32_t softCrcStep(uint32_t state, uint8_t byte) {
  state ^= byte;
  for (uint8_t i = 0; i < 8; ++i) {
    state = (state & 1U) ? ((state >> 1U) ^ Crc32Spec::POLY_REFLECTED)
                         : (state >> 1U);
  }
  return state;
}

// Portable CRC-32/IEEE. CMSIS-free and reentrant -- unlike the single hardware
// unit it carries no global state, so it runs on chips without a CRC block, in
// host tests and inside consteval checks. Bitwise (8 steps per byte) to keep
// .rodata at zero; a nibble or byte table can replace the loop later without
// touching this API.
class SoftCrc {
public:
  constexpr void reset() { _state = Crc32Spec::INIT; }

  constexpr void update(std::span<const uint8_t> data) {
    for (size_t i = 0, n = data.size(); i < n; ++i) {
      _state = softCrcStep(_state, data[i]);
    }
  }

  [[nodiscard]] constexpr uint32_t value() const {
    return _state ^ Crc32Spec::XOR_OUT;
  }

  [[nodiscard]] constexpr uint32_t compute(std::span<const uint8_t> data) {
    reset();
    update(data);
    return value();
  }

private:
  uint32_t _state = Crc32Spec::INIT;
};

static_assert(ICrc<SoftCrc>, "SoftCrc must model driver::ICrc");

}  // namespace driver

namespace driver::detail {

// Known-answer self-check in the style of resultSelfCheck (driver.types): if
// the polynomial, the seed or the streaming logic ever drifts, the build stops
// here instead of silently producing checksums nothing else can verify.
consteval bool softCrcSelfCheck() {
  const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

  SoftCrc oneShot;
  if (oneShot.compute({check, 9}) != Crc32Spec::CHECK) {
    return false;
  }

  SoftCrc empty;
  if (empty.compute({}) != 0x00000000) {
    return false;
  }

  // Chunked feeding must be indistinguishable from one call -- this is what
  // lets a partition be hashed page by page.
  SoftCrc streamed;
  streamed.reset();
  streamed.update({check, 4});
  streamed.update({check + 4, 5});
  if (streamed.value() != Crc32Spec::CHECK) {
    return false;
  }

  // value() is a pure read: calling it twice must not consume the state.
  return streamed.value() == Crc32Spec::CHECK;
}

static_assert(softCrcSelfCheck(), "SoftCrc does not match CRC-32/IEEE vectors");

}  // namespace driver::detail
