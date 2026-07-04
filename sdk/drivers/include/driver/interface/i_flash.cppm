module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.flash;

import driver.types;

export namespace driver {

// Compile-time contract for internal flash (replaces the former virtual
// IFlash base class). `sectorSize` / `sectorCount` are checked through a
// const object.
template <typename T>
concept IFlash = requires(
    T flash,
    const T cflash,
    uint32_t addr,
    uint8_t sector,
    std::span<const uint8_t> wr,
    std::span<uint8_t> rd
) {
  { flash.read(addr, rd) } -> std::same_as<Status>;
  { flash.write(addr, wr) } -> std::same_as<Status>;
  { flash.eraseSector(sector) } -> std::same_as<Status>;
  { cflash.sectorSize(sector) } -> std::same_as<size_t>;
  { cflash.sectorCount() } -> std::same_as<uint8_t>;
};

}  // namespace driver
