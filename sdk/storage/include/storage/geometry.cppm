module;
#include <concepts>
#include <cstdint>
export module storage.geometry;

export namespace storage {

// A device whose sectors are all the same size, described by two compile-time
// constants. sensor::W25q32Spec already satisfies this as-is (CAPACITY +
// SECTOR_SIZE), which is why partitionTable<W25q32Spec>({...}) works without
// touching the sensor.
template <typename S>
concept UniformSpec = requires {
  { S::CAPACITY } -> std::convertible_to<uint32_t>;
  { S::SECTOR_SIZE } -> std::convertible_to<uint32_t>;
};

// The full contract the partition validator needs. Erase granularity is the
// sector, so a partition may only start and end on a sector boundary --
// otherwise erasing it would take a bite out of its neighbour.
template <typename G>
concept IFlashGeometry = requires(uint32_t off) {
  { G::CAPACITY } -> std::convertible_to<uint32_t>;
  { G::isSectorBoundary(off) } -> std::same_as<bool>;
  { G::sectorSizeAt(off) } -> std::same_as<uint32_t>;
};

template <UniformSpec S>
struct UniformGeometry {
  static constexpr uint32_t CAPACITY = S::CAPACITY;
  static constexpr uint32_t SECTOR_SIZE = S::SECTOR_SIZE;

  static constexpr bool isSectorBoundary(uint32_t off) {
    return (off % SECTOR_SIZE) == 0;
  }

  static constexpr uint32_t sectorSizeAt(uint32_t) { return SECTOR_SIZE; }

  static constexpr uint32_t sectorBase(uint32_t off) {
    return off - (off % SECTOR_SIZE);
  }
};

namespace detail {

// STM32F4 main-memory layout: 4x16K, 1x64K, then 128K sectors. Mirrors
// driver::stm32f4::InternalFlash::sectorSize(), which is a runtime method and
// therefore unusable in a constant expression; the self-check at the bottom of
// this file keeps the two in step.
constexpr uint32_t f4SectorSize(uint8_t sector) {
  if (sector < 4) {
    return 16U * 1024U;
  }
  if (sector == 4) {
    return 64U * 1024U;
  }
  return 128U * 1024U;
}

constexpr uint32_t f4Capacity(uint8_t sectorCount) {
  uint32_t total = 0;
  for (uint8_t s = 0; s < sectorCount; ++s) {
    total += f4SectorSize(s);
  }
  return total;
}

}  // namespace detail

// Compile-time description of the F4 internal flash. SectorCount matches the
// argument given to driver::stm32f4::InternalFlash (12 on a 1 MB part).
template <uint8_t SectorCount = 12>
struct Stm32f4Geometry {
  static_assert(SectorCount > 0, "Stm32f4Geometry needs at least one sector");
  static_assert(
      SectorCount <= 12,
      "STM32F4 has at most 12 main-memory sectors"
  );

  static constexpr uint32_t CAPACITY = detail::f4Capacity(SectorCount);
  static constexpr uint8_t SECTOR_COUNT = SectorCount;

  static constexpr uint32_t sectorSize(uint8_t sector) {
    return detail::f4SectorSize(sector);
  }

  // The end of the last sector counts as a boundary: a partition may run to
  // the very top of the device.
  static constexpr bool isSectorBoundary(uint32_t off) {
    uint32_t base = 0;
    for (uint8_t s = 0; s < SectorCount; ++s) {
      if (off == base) {
        return true;
      }
      base += sectorSize(s);
    }
    return off == base;
  }

  static constexpr uint32_t sectorSizeAt(uint32_t off) {
    uint32_t base = 0;
    for (uint8_t s = 0; s < SectorCount; ++s) {
      const uint32_t size = sectorSize(s);
      if (off < base + size) {
        return size;
      }
      base += size;
    }
    return 0;
  }

  static constexpr uint8_t sectorIndexAt(uint32_t off) {
    uint32_t base = 0;
    for (uint8_t s = 0; s < SectorCount; ++s) {
      const uint32_t size = sectorSize(s);
      if (off < base + size) {
        return s;
      }
      base += size;
    }
    return SectorCount;
  }

  static constexpr uint32_t sectorBase(uint32_t off) {
    uint32_t base = 0;
    for (uint8_t s = 0; s < SectorCount; ++s) {
      const uint32_t size = sectorSize(s);
      if (off < base + size) {
        return base;
      }
      base += size;
    }
    return base;
  }
};

// Accepts either a full geometry or a uniform spec, so callers may write
// partitionTable<W25q32Spec> as well as partitionTable<Stm32f4Geometry<12>>.
// A plain conditional_t would not do: naming UniformGeometry<S> for an S that
// is not a UniformSpec is itself an error, so the wrapper must only be formed
// in the branch that needs it.
namespace detail {

template <typename S, bool = IFlashGeometry<S>>
struct GeometrySelector {
  using type = UniformGeometry<S>;
};

template <typename S>
struct GeometrySelector<S, true> {
  using type = S;
};

}  // namespace detail

template <typename S>
using GeometryOf = typename detail::GeometrySelector<S>::type;

}  // namespace storage

namespace storage::detail {

struct UniformProbe {
  static constexpr uint32_t CAPACITY = 4U * 4096U;
  static constexpr uint32_t SECTOR_SIZE = 4096U;
};

consteval bool geometrySelfCheck() {
  using Uniform = GeometryOf<UniformProbe>;
  static_assert(
      IFlashGeometry<Uniform>,
      "UniformGeometry must model IFlashGeometry"
  );
  if (Uniform::CAPACITY != 16384U || !Uniform::isSectorBoundary(8192U)) {
    return false;
  }
  if (Uniform::isSectorBoundary(1U) || Uniform::sectorSizeAt(9000U) != 4096U) {
    return false;
  }

  using F4 = Stm32f4Geometry<12>;
  static_assert(
      std::same_as<GeometryOf<F4>, F4>,
      "a full geometry must pass through GeometryOf unchanged"
  );

  // 4*16K + 64K + 7*128K = 1 MB.
  if (F4::CAPACITY != 1024U * 1024U) {
    return false;
  }
  // Boundaries of the mixed-size region: sector 4 starts at 64K and is 64K
  // long, so sector 5 starts at 128K.
  if (!F4::isSectorBoundary(0) || !F4::isSectorBoundary(64U * 1024U)) {
    return false;
  }
  if (!F4::isSectorBoundary(128U * 1024U) ||
      !F4::isSectorBoundary(F4::CAPACITY)) {
    return false;
  }
  if (F4::isSectorBoundary(16U * 1024U + 1U)) {
    return false;
  }
  if (F4::sectorSizeAt(0) != 16U * 1024U ||
      F4::sectorSizeAt(64U * 1024U) != 64U * 1024U) {
    return false;
  }
  if (F4::sectorSizeAt(128U * 1024U) != 128U * 1024U) {
    return false;
  }
  if (F4::sectorIndexAt(0) != 0 || F4::sectorIndexAt(64U * 1024U) != 4) {
    return false;
  }
  if (F4::sectorIndexAt(128U * 1024U) != 5 ||
      F4::sectorBase(200U * 1024U) != 128U * 1024U) {
    return false;
  }
  return true;
}

static_assert(geometrySelfCheck(), "flash geometry invariants broken");

}  // namespace storage::detail
