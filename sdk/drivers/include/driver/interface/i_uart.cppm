module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module driver.uart;

import driver.types;

export namespace driver {

enum class Parity : uint8_t {
    None,
    Even,
    Odd
};

// Compile-time contract for a UART (replaces the former virtual IUart base
// class). `rxAvailable` / `txFree` are checked through a const object.
template <typename T>
concept IUart = requires(T uart, const T cuart, std::span<const uint8_t> wr,
                         std::span<uint8_t> rd) {
    { uart.write(wr) } -> std::same_as<size_t>;
    { uart.read(rd) } -> std::same_as<size_t>;
    { uart.writeNonBlocking(wr) } -> std::same_as<size_t>;
    { uart.readNonBlocking(rd) } -> std::same_as<size_t>;
    { cuart.rxAvailable() } -> std::same_as<size_t>;
    { cuart.txFree() } -> std::same_as<size_t>;
    { uart.irqHandler() } -> std::same_as<void>;
};

}  // namespace driver
