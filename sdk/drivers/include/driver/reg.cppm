module;
#include <cstdint>
export module driver.reg;

export namespace driver::reg {

inline void set(volatile uint32_t &reg, uint32_t bits) {
  reg |= bits;
}

inline void clear(volatile uint32_t &reg, uint32_t bits) {
  reg &= ~bits;
}

inline bool read(volatile uint32_t &reg, uint32_t bits) {
  return (reg & bits) != 0;
}

inline uint32_t get(volatile uint32_t &reg) {
  return reg;
}

inline void write(volatile uint32_t &reg, uint32_t val) {
  reg = val;
}

inline void modify(volatile uint32_t &reg, uint32_t clrMask, uint32_t setMask) {
  reg = (reg & ~clrMask) | setMask;
}

inline uint32_t positionVal(uint32_t val) {
  uint32_t pos = 0;
  while (pos < 32 && !(val & (1U << pos))) {
    ++pos;
  }
  return pos;
}

}  // namespace driver::reg
