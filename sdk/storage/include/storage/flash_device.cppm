module;
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
export module storage.flash_device;

import driver.crc;
import driver.flash;
import driver.types;
import storage.geometry;
import storage.partition;

export namespace storage {

// What the partition layer needs from a medium: offset addressing from zero,
// writes of any length, erase by byte range. Neither driver::IFlash (absolute
// addresses, erase by sector index) nor an external SPI flash (erase by
// address, writes confined to one page) offers this directly, hence the two
// adapters below.
template <typename T>
concept IFlashDevice = requires(
    T dev,
    const T cdev,
    uint32_t off,
    uint32_t len,
    std::span<const uint8_t> wr,
    std::span<uint8_t> rd
) {
  { dev.read(off, rd) } -> std::same_as<driver::Status>;
  { dev.write(off, wr) } -> std::same_as<driver::Status>;
  { dev.erase(off, len) } -> std::same_as<driver::Status>;
  { cdev.capacity() } -> std::same_as<uint32_t>;
  { cdev.sectorSizeAt(off) } -> std::same_as<uint32_t>;
  { cdev.erasedValue() } -> std::same_as<uint8_t>;
};

// Structural requirements for an external SPI flash. Deliberately stated here
// rather than importing sensor.external_flash: that would tie the storage
// layer to STM32_USE_SENSORS. sensor::W25q32 satisfies this as written.
template <typename T>
concept ExternalFlashLike = requires(
    T flash,
    const T cflash,
    uint32_t addr,
    std::span<const uint8_t> wr,
    std::span<uint8_t> rd
) {
  { flash.read(addr, rd) } -> std::same_as<driver::Status>;
  { flash.writePage(addr, wr) } -> std::same_as<driver::Status>;
  { flash.eraseSector(addr) } -> std::same_as<driver::Status>;
  { cflash.capacity() } -> std::same_as<uint32_t>;
  { cflash.sectorSize() } -> std::same_as<uint32_t>;
  { cflash.pageSize() } -> std::same_as<uint32_t>;
};

// A geometry that can also name sectors by index -- what driver::IFlash needs
// for eraseSector(uint8_t). Stm32f4Geometry models it; UniformGeometry does
// not, because a 4 MB chip with 4 KB sectors has more sectors than a uint8_t
// can index and no internal-flash driver takes that shape anyway.
template <typename G>
concept SectorIndexedGeometry = IFlashGeometry<G> && requires(uint32_t off) {
  { G::sectorIndexAt(off) } -> std::same_as<uint8_t>;
  { G::sectorBase(off) } -> std::same_as<uint32_t>;
};

// Adapter over an external SPI flash. Adds the multi-page write the chip
// driver lacks: programming may not cross a page boundary, so a long buffer is
// split at every page edge.
template <ExternalFlashLike Flash>
class ExternalFlashDevice {
public:
  explicit ExternalFlashDevice(Flash &flash) : _flash(flash) {}

  [[nodiscard]] driver::Status read(uint32_t off, std::span<uint8_t> out) {
    return _flash.read(off, out);
  }

  [[nodiscard]] driver::Status
  write(uint32_t off, std::span<const uint8_t> in) {
    const uint32_t page = _flash.pageSize();
    size_t done = 0;
    while (done < in.size()) {
      const uint32_t addr = off + static_cast<uint32_t>(done);
      const size_t room = page - (addr % page);
      size_t chunk = in.size() - done;
      if (chunk > room) {
        chunk = room;
      }
      const driver::Status status =
          _flash.writePage(addr, in.subspan(done, chunk));
      if (status != driver::Status::Ok) {
        return status;
      }
      done += chunk;
    }
    return driver::Status::Ok;
  }

  [[nodiscard]] driver::Status erase(uint32_t off, uint32_t len) {
    const uint32_t sector = _flash.sectorSize();
    for (uint32_t addr = off; addr < off + len; addr += sector) {
      const driver::Status status = _flash.eraseSector(addr);
      if (status != driver::Status::Ok) {
        return status;
      }
    }
    return driver::Status::Ok;
  }

  [[nodiscard]] uint32_t capacity() const { return _flash.capacity(); }

  [[nodiscard]] uint32_t sectorSizeAt(uint32_t) const {
    return _flash.sectorSize();
  }

  [[nodiscard]] uint8_t erasedValue() const { return 0xFF; }

private:
  Flash &_flash;
};

// Adapter over the internal flash. Translates an offset into the absolute
// address driver::IFlash expects, and a byte range into the sector indices its
// eraseSector() takes.
template <driver::IFlash Flash, SectorIndexedGeometry Geometry>
class InternalFlashDevice {
public:
  static constexpr uint32_t DEFAULT_BASE = 0x08000000;

  explicit InternalFlashDevice(
      Flash &flash,
      uint32_t baseAddress = DEFAULT_BASE
  )
      : _flash(flash), _base(baseAddress) {}

  [[nodiscard]] driver::Status read(uint32_t off, std::span<uint8_t> out) {
    return _flash.read(_base + off, out);
  }

  [[nodiscard]] driver::Status
  write(uint32_t off, std::span<const uint8_t> in) {
    return _flash.write(_base + off, in);
  }

  [[nodiscard]] driver::Status erase(uint32_t off, uint32_t len) {
    uint32_t addr = off;
    while (addr < off + len) {
      const driver::Status status =
          _flash.eraseSector(Geometry::sectorIndexAt(addr));
      if (status != driver::Status::Ok) {
        return status;
      }
      addr = Geometry::sectorBase(addr) + Geometry::sectorSizeAt(addr);
    }
    return driver::Status::Ok;
  }

  [[nodiscard]] uint32_t capacity() const { return Geometry::CAPACITY; }

  [[nodiscard]] uint32_t sectorSizeAt(uint32_t off) const {
    return Geometry::sectorSizeAt(off);
  }

  [[nodiscard]] uint8_t erasedValue() const { return 0xFF; }

private:
  Flash &_flash;
  uint32_t _base;
};

// A view of one partition on a device. Offsets are relative to the partition,
// the way Zephyr's flash_area works, and anything reaching past its end is
// rejected with Status::InvalidArg rather than silently landing in a
// neighbour.
template <IFlashDevice Device>
class Partition {
public:
  constexpr Partition(Device &device, const PartitionSpec &spec)
      : _device(device), _spec(spec) {}

  [[nodiscard]] driver::Status read(uint32_t off, std::span<uint8_t> out)
      const {
    if (!fits(off, static_cast<uint32_t>(out.size()))) {
      return driver::Status::InvalidArg;
    }
    return _device.read(_spec.offset + off, out);
  }

  [[nodiscard]] driver::Status
  write(uint32_t off, std::span<const uint8_t> in) {
    if (!fits(off, static_cast<uint32_t>(in.size()))) {
      return driver::Status::InvalidArg;
    }
    return _device.write(_spec.offset + off, in);
  }

  [[nodiscard]] driver::Status erase(uint32_t off, uint32_t len) {
    if (!fits(off, len)) {
      return driver::Status::InvalidArg;
    }
    return _device.erase(_spec.offset + off, len);
  }

  [[nodiscard]] driver::Status erase() { return erase(0, _spec.size); }

  // Streams the whole partition through a CRC engine without a staging
  // buffer -- this is why driver::ICrc keeps reset/update/value separate.
  template <driver::ICrc CrcEngine>
  [[nodiscard]] driver::Result<uint32_t> checksum(CrcEngine &crc) const {
    crc.reset();
    uint8_t buffer[CHECKSUM_CHUNK];
    uint32_t done = 0;
    while (done < _spec.size) {
      uint32_t chunk = _spec.size - done;
      if (chunk > CHECKSUM_CHUNK) {
        chunk = CHECKSUM_CHUNK;
      }
      const driver::Status status =
          _device.read(_spec.offset + done, {buffer, chunk});
      if (status != driver::Status::Ok) {
        return status;
      }
      crc.update({buffer, chunk});
      done += chunk;
    }
    return crc.value();
  }

  [[nodiscard]] constexpr uint32_t size() const { return _spec.size; }
  [[nodiscard]] constexpr uint32_t deviceOffset() const { return _spec.offset; }
  [[nodiscard]] constexpr const char *name() const { return _spec.name; }
  [[nodiscard]] uint8_t erasedValue() const { return _device.erasedValue(); }

private:
  static constexpr uint32_t CHECKSUM_CHUNK = 64;

  [[nodiscard]] constexpr bool fits(uint32_t off, uint32_t len) const {
    return off <= _spec.size && len <= _spec.size - off;
  }

  Device &_device;
  PartitionSpec _spec;
};

}  // namespace storage
