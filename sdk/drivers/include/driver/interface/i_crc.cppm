module;
#include <concepts>
#include <cstdint>
#include <span>
export module driver.crc;

export namespace driver {

// CRC-32/IEEE parameters -- the variant used by zlib, gzip, PNG and Ethernet,
// and by Zephyr's crc32_ieee(). Picked for interoperability: a flash dump can
// be checked on a workstation with
//   python3 -c 'import zlib;
//   print(hex(zlib.crc32(open("dump.bin","rb").read())))'
// no matter which flash chip produced it.
//
// The STM32F4 CRC unit computes the same polynomial but MSB-first, without
// input/output reflection and without the final XOR (CRC_CR holds a single
// RESET bit -- nothing is configurable). driver::stm32f4::Crc bridges the two
// domains with __RBIT; see driver.stm32f4.crc.
struct Crc32Spec {
  static constexpr uint32_t POLY_REFLECTED = 0xEDB88320;
  static constexpr uint32_t INIT = 0xFFFFFFFF;
  static constexpr uint32_t XOR_OUT = 0xFFFFFFFF;
  // Standard check value of the algorithm: CRC-32 of the ASCII "123456789".
  static constexpr uint32_t CHECK = 0xCBF43926;
};

// Compile-time contract for a CRC-32 engine. Modelled by driver::SoftCrc and
// driver::stm32f4::Crc without inheritance (same concept style as IExti).
// `reset`/`update`/`value` are the streaming triplet -- data arriving in
// chunks (a flash partition read page by page) never needs a staging buffer;
// `compute` is the one-shot convenience. `value` is checked through a const
// object because reading the result must not disturb the accumulator.
template <typename T>
concept ICrc = requires(T crc, const T ccrc, std::span<const uint8_t> data) {
  { crc.reset() } -> std::same_as<void>;
  { crc.update(data) } -> std::same_as<void>;
  { ccrc.value() } -> std::same_as<uint32_t>;
  { crc.compute(data) } -> std::same_as<uint32_t>;
};

}  // namespace driver
