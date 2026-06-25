module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.i2c;

import driver.types;

export namespace driver {

// Compile-time contract for an I2C master bus (replaces the former virtual
// II2c base class). A sensor templated on `II2c Bus` calls these directly.
template <typename T>
concept II2c = requires(T bus, uint8_t addr, uint8_t reg, std::span<const uint8_t> wr,
                        std::span<uint8_t> rd) {
    { bus.write(addr, wr) } -> std::same_as<Status>;
    { bus.read(addr, rd) } -> std::same_as<Status>;
    { bus.writeReg(addr, reg, wr) } -> std::same_as<Status>;
    { bus.readReg(addr, reg, rd) } -> std::same_as<Status>;
    { bus.probe(addr) } -> std::same_as<Status>;
};

}  // namespace driver
