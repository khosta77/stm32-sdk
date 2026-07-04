module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
export module sensor.ssd1306;

import driver.types;
import driver.i2c;
import sensor.display;
import sensor.font5x7;

export namespace sensor {

template <driver::II2c I2cDriver>
class Ssd1306 {
public:
  struct Config {
    uint8_t addr;      // 7-bit, 0x3C (default) or 0x3D (when SA0 tied high)
    uint8_t contrast;  // 0..255
    bool flipH;        // mirror columns (segment remap)
    bool flipV;        // mirror rows (com scan direction)
  };

  static constexpr uint16_t WIDTH = 128;
  static constexpr uint16_t HEIGHT = 64;
  static constexpr uint16_t PAGES = HEIGHT / 8;
  static constexpr size_t FRAMEBUFFER_SIZE = static_cast<size_t>(WIDTH) * PAGES;

private:
  I2cDriver &_i2c;
  Config _cfg;
  uint8_t _fb[FRAMEBUFFER_SIZE]{};

  enum CtrlByte : uint8_t {
    CMD_STREAM = 0x00,   // Co=0, D/C=0 → all following bytes are commands
    DATA_STREAM = 0x40,  // Co=0, D/C=1 → all following bytes are data
  };

public:
  Ssd1306(I2cDriver &i2c, const Config &cfg) : _i2c(i2c), _cfg(cfg) {}

  Ssd1306(const Ssd1306 &) = delete;
  Ssd1306 &operator=(const Ssd1306 &) = delete;

  [[nodiscard]] driver::Status init() {
    const uint8_t segRemap = static_cast<uint8_t>(_cfg.flipH ? 0xA1 : 0xA0);
    const uint8_t comScan = static_cast<uint8_t>(_cfg.flipV ? 0xC8 : 0xC0);

    const uint8_t initSeq[] = {
        0xAE,                     // DISPLAY_OFF
        0xD5,     0x80,           // CLOCK_DIV: ratio 1, freq 8
        0xA8,     0x3F,           // MULTIPLEX: 64 rows
        0xD3,     0x00,           // DISPLAY_OFFSET: 0
        0x40,                     // START_LINE: 0
        0x8D,     0x14,           // CHARGE_PUMP: enabled (internal)
        0x20,     0x00,           // ADDR_MODE: horizontal
        segRemap,                 // SEGMENT_REMAP
        comScan,                  // COM_SCAN_DIR
        0xDA,     0x12,           // COM_PINS: alternative, no remap
        0x81,     _cfg.contrast,  // CONTRAST
        0xD9,     0xF1,           // PRECHARGE
        0xDB,     0x40,           // VCOMH_DESELECT
        0xA4,                     // RESUME (use RAM, not all-on)
        0xA6,                     // NORMAL (not inverse)
        0x2E,                     // DEACTIVATE_SCROLL
        0xAF,                     // DISPLAY_ON
    };
    return sendCommands({initSeq, sizeof(initSeq)});
  }

  void clear() { std::memset(_fb, 0, FRAMEBUFFER_SIZE); }

  void setPixel(uint16_t x, uint16_t y, bool on) {
    if (x >= WIDTH || y >= HEIGHT) {
      return;
    }
    const size_t idx = static_cast<size_t>(y / 8) * WIDTH + x;
    const uint8_t bit = static_cast<uint8_t>(1U << (y % 8));
    if (on) {
      _fb[idx] |= bit;
    } else {
      _fb[idx] = static_cast<uint8_t>(_fb[idx] & ~bit);
    }
  }

  void drawChar(uint16_t x, uint16_t y, char c) {
    const auto code = static_cast<uint8_t>(c);
    if (code < font5x7::firstChar || code > font5x7::lastChar) {
      return;
    }
    const auto &glyph = font5x7::table[code - font5x7::firstChar];
    for (uint8_t col = 0; col < font5x7::glyphWidth; ++col) {
      const uint8_t bits = glyph[col];
      for (uint8_t row = 0; row < font5x7::glyphHeight; ++row) {
        if (bits & (1U << row)) {
          setPixel(
              static_cast<uint16_t>(x + col),
              static_cast<uint16_t>(y + row),
              true
          );
        }
      }
    }
  }

  void drawText(uint16_t x, uint16_t y, const char *text) {
    uint16_t cursor = x;
    while (text != nullptr && *text != '\0') {
      if (cursor + font5x7::glyphWidth > WIDTH) {
        break;
      }
      drawChar(cursor, y, *text);
      cursor = static_cast<uint16_t>(cursor + font5x7::advance);
      ++text;
    }
  }

  [[nodiscard]] driver::Status flush() {
    const uint8_t addrSetup[] = {
        0x21,
        0x00,
        static_cast<uint8_t>(WIDTH - 1),  // COLUMN_ADDR: 0..127
        0x22,
        0x00,
        static_cast<uint8_t>(PAGES - 1),  // PAGE_ADDR: 0..7
    };
    auto st = sendCommands({addrSetup, sizeof(addrSetup)});
    if (st != driver::Status::Ok) {
      return st;
    }
    return _i2c.writeReg(_cfg.addr, DATA_STREAM, {_fb, FRAMEBUFFER_SIZE});
  }

  [[nodiscard]] uint16_t width() const { return WIDTH; }
  [[nodiscard]] uint16_t height() const { return HEIGHT; }

private:
  driver::Status sendCommands(std::span<const uint8_t> cmds) {
    // Index loop, not range-for: across a module boundary GCC 15 fails to
    // find operator!= for std::span<>::iterator when this template member
    // is instantiated in the consumer TU.
    for (size_t i = 0; i < cmds.size(); ++i) {
      const uint8_t cmd = cmds[i];
      const auto st = _i2c.writeReg(_cfg.addr, CMD_STREAM, {&cmd, 1});
      if (st != driver::Status::Ok) {
        return st;
      }
    }
    return driver::Status::Ok;
  }
};

}  // namespace sensor

namespace sensor::detail {

// Zero-infra compile-time check (not exported): Ssd1306 on a trivial II2c bus
// models sensor::IDisplay.
struct MockI2c {
  driver::Status write(uint8_t, std::span<const uint8_t>) {
    return driver::Status::Ok;
  }
  driver::Status read(uint8_t, std::span<uint8_t>) {
    return driver::Status::Ok;
  }
  driver::Status writeReg(uint8_t, uint8_t, std::span<const uint8_t>) {
    return driver::Status::Ok;
  }
  driver::Status readReg(uint8_t, uint8_t, std::span<uint8_t>) {
    return driver::Status::Ok;
  }
  driver::Status probe(uint8_t) { return driver::Status::Ok; }
};
static_assert(
    IDisplay<Ssd1306<MockI2c>>,
    "Ssd1306 must model sensor::IDisplay"
);

}  // namespace sensor::detail
