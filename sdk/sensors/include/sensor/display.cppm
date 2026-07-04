module;
#include <concepts>
#include <cstddef>
#include <cstdint>
export module sensor.display;

import driver.types;

export namespace sensor {

// Compile-time contract for a monochrome display (replaces the former virtual
// IDisplay base class). `width` / `height` are checked through a const object.
template <typename T>
concept IDisplay = requires(
    T disp,
    const T cdisp,
    uint16_t x,
    uint16_t y,
    bool on,
    char c,
    const char *text
) {
  { disp.init() } -> std::same_as<driver::Status>;
  { disp.clear() } -> std::same_as<void>;
  { disp.setPixel(x, y, on) } -> std::same_as<void>;
  { disp.drawChar(x, y, c) } -> std::same_as<void>;
  { disp.drawText(x, y, text) } -> std::same_as<void>;
  { disp.flush() } -> std::same_as<driver::Status>;
  { cdisp.width() } -> std::same_as<uint16_t>;
  { cdisp.height() } -> std::same_as<uint16_t>;
};

}  // namespace sensor
