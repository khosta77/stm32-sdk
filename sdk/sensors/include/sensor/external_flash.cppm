module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module sensor.external_flash;

import driver.types;

export namespace sensor {

// Compile-time contract for external (SPI/QSPI) flash (replaces the former
// virtual IExternalFlash base class). Const accessors are checked through a
// const object.
template <typename T>
concept IExternalFlash = requires(
    T flash,
    const T cflash,
    uint32_t addr,
    std::span<const uint8_t> wr,
    std::span<uint8_t> rd
) {
  { flash.init() } -> std::same_as<driver::Status>;
  { flash.read(addr, rd) } -> std::same_as<driver::Status>;
  { flash.writePage(addr, wr) } -> std::same_as<driver::Status>;
  { flash.eraseSector(addr) } -> std::same_as<driver::Status>;
  { flash.chipErase() } -> std::same_as<driver::Status>;
  { cflash.capacity() } -> std::same_as<uint32_t>;
  { cflash.sectorSize() } -> std::same_as<uint32_t>;
  { cflash.pageSize() } -> std::same_as<uint32_t>;
  { cflash.jedecId() } -> std::same_as<uint32_t>;
};

}  // namespace sensor
