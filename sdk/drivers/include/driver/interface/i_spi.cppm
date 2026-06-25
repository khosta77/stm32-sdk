module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.spi;

import driver.types;

export namespace driver {

// Compile-time contract for an SPI master bus (replaces the former virtual
// ISpi base class).
template <typename T>
concept ISpi = requires(T bus, std::span<const uint8_t> tx, std::span<uint8_t> rx) {
    { bus.transfer(tx, rx) } -> std::same_as<Status>;
    { bus.write(tx) } -> std::same_as<Status>;
    { bus.read(rx) } -> std::same_as<Status>;
};

}  // namespace driver
