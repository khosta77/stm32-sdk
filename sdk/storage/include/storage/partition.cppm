module;
#include <cstddef>
#include <cstdint>
export module storage.partition;

import storage.geometry;

export namespace storage {

// Names are stored inline rather than as `const char *` because the whole map
// is a template argument, and a pointer to a string literal is not a valid
// one ("is not a variable or function"). 15 usable characters plus the NUL.
inline constexpr size_t PARTITION_NAME_MAX = 16;

struct PartitionName {
  char text[PARTITION_NAME_MAX]{};

  // Deduces the literal's length, so both bounds are constant expressions and
  // can be asserted on directly.
  template <size_t N>
  consteval PartitionName(const char (&literal)[N]) {
    static_assert(N > 1, "PartitionName: partition name must not be empty");
    static_assert(
        N <= PARTITION_NAME_MAX,
        "PartitionName: partition name is too long (15 characters max)"
    );
    for (size_t i = 0; i < N; ++i) {
      text[i] = literal[i];
    }
  }

  [[nodiscard]] constexpr bool operator==(const PartitionName &other) const {
    for (size_t i = 0; i < PARTITION_NAME_MAX; ++i) {
      if (text[i] != other.text[i]) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] constexpr const char *c_str() const { return text; }
};

// One entry of the partition map. Offsets are absolute within the device; the
// Partition view built on top of an entry addresses relative to it, the way
// Zephyr's flash_area does.
struct PartitionSpec {
  PartitionName name;
  uint32_t offset;
  uint32_t size;
};

// Null-safe literal comparison, kept for callers holding a plain pointer --
// Partition::name() hands one out. <string_view> is not part of the SDK's
// freestanding STL surface.
constexpr bool nameEquals(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) {
    return a == b;
  }
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

}  // namespace storage

namespace storage::detail {

// Pairwise invariants do not fit a fold expression, so they are constexpr
// predicates over the pack instead. Each keeps its own static_assert message
// at the use site.
template <PartitionSpec... Specs>
consteval bool noDuplicateNames() {
  constexpr PartitionSpec entries[]{Specs...};
  for (size_t i = 0; i < sizeof...(Specs); ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (entries[i].name == entries[j].name) {
        return false;
      }
    }
  }
  return true;
}

template <PartitionSpec... Specs>
consteval bool noOverlaps() {
  constexpr PartitionSpec entries[]{Specs...};
  for (size_t i = 0; i < sizeof...(Specs); ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (entries[i].offset < entries[j].offset + entries[j].size &&
          entries[j].offset < entries[i].offset + entries[i].size) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace storage::detail

export namespace storage {

// A validated partition map. Spec may be a uniform spec (sensor::W25q32Spec)
// or a full geometry (storage::Stm32f4Geometry<12>) -- GeometryOf sorts that
// out.
//
// Rejected at compile time: an empty map, empty or duplicated names, zero
// size, partitions that do not start and end on a sector boundary, partitions
// that run past the device, and any pair that overlaps. Order is free-form:
// validation is pairwise, not sequential.
template <typename Spec, PartitionSpec... Specs>
struct PartitionMap {
  using Geometry = GeometryOf<Spec>;

  static constexpr size_t COUNT = sizeof...(Specs);
  static constexpr PartitionSpec ENTRIES[COUNT > 0 ? COUNT : 1]{Specs...};

  static_assert(
      IFlashGeometry<Geometry>,
      "PartitionMap: Spec must be a UniformSpec or model IFlashGeometry"
  );
  static_assert(COUNT > 0, "PartitionMap: the map must not be empty");
  static_assert(
      ((Specs.size != 0) && ...),
      "PartitionMap: partition size must be non-zero"
  );
  static_assert(
      ((Specs.offset <= Geometry::CAPACITY &&
        Specs.size <= Geometry::CAPACITY - Specs.offset) &&
       ...),
      "PartitionMap: partition does not fit in the device"
  );
  static_assert(
      (Geometry::isSectorBoundary(Specs.offset) && ...),
      "PartitionMap: partition offset must be sector-aligned"
  );
  static_assert(
      (Geometry::isSectorBoundary(Specs.offset + Specs.size) && ...),
      "PartitionMap: partition end must be sector-aligned (erasing it would "
      "otherwise touch the next partition)"
  );
  static_assert(
      detail::noDuplicateNames<Specs...>(),
      "PartitionMap: duplicate partition name"
  );
  static_assert(
      detail::noOverlaps<Specs...>(),
      "PartitionMap: partitions overlap"
  );

  // consteval on purpose: a name that is not in the map is a compile error,
  // never a runtime lookup failure. Same idea as gpio()/spi()/exti().
  template <PartitionName Name>
  [[nodiscard]] static consteval PartitionSpec find() {
    static_assert(
        ((Specs.name == Name) || ...),
        "PartitionMap: no partition with this name"
    );
    for (size_t i = 0; i < COUNT; ++i) {
      if (ENTRIES[i].name == Name) {
        return ENTRIES[i];
      }
    }
    return ENTRIES[0];
  }

  [[nodiscard]] static constexpr size_t count() { return COUNT; }

  [[nodiscard]] static constexpr const PartitionSpec &at(size_t i) {
    return ENTRIES[i];
  }
};

}  // namespace storage

namespace storage::detail {

struct PartitionProbe {
  static constexpr uint32_t CAPACITY = 8U * 4096U;
  static constexpr uint32_t SECTOR_SIZE = 4096U;
};

using ProbeMap = PartitionMap<
    PartitionProbe,
    {.name = "boot", .offset = 0, .size = 4096},
    {.name = "config", .offset = 4096, .size = 4096},
    {.name = "storage", .offset = 8192, .size = 8192 + 8192}>;

// Order in the map is free-form: validation is pairwise, not sequential.
using UnorderedProbeMap = PartitionMap<
    PartitionProbe,
    {.name = "tail", .offset = 4096, .size = 4096},
    {.name = "head", .offset = 0, .size = 4096}>;

consteval bool partitionMapSelfCheck() {
  if (ProbeMap::count() != 3) {
    return false;
  }
  if (ProbeMap::find<"boot">().offset != 0 ||
      ProbeMap::find<"boot">().size != 4096) {
    return false;
  }
  if (ProbeMap::find<"storage">().offset != 8192) {
    return false;
  }
  if (ProbeMap::at(1).offset != 4096) {
    return false;
  }
  if (UnorderedProbeMap::find<"head">().offset != 0) {
    return false;
  }
  if (!(PartitionName{"abc"} == PartitionName{"abc"}) ||
      PartitionName{"abc"} == PartitionName{"abd"} ||
      PartitionName{"abc"} == PartitionName{"ab"}) {
    return false;
  }

  return nameEquals("abc", "abc") && !nameEquals("abc", "abd") &&
         !nameEquals("abc", "ab") && !nameEquals("ab", "abc");
}

static_assert(partitionMapSelfCheck(), "PartitionMap invariants broken");

}  // namespace storage::detail
