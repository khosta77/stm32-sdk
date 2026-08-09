module;
#include <array>
#include <cstddef>
#include <cstdint>
export module storage.partition;

import storage.geometry;

export namespace storage {

// One entry of the partition map. Offsets are absolute within the device; the
// Partition view built on top of an entry addresses relative to it, the way
// Zephyr's flash_area does.
struct PartitionSpec {
  const char *name;
  uint32_t offset;
  uint32_t size;
};

// Null-safe literal comparison. <string_view> is not part of the SDK's
// freestanding STL surface, and a partition name is a plain literal.
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

// Validated map. Built exclusively through partitionTable(), which is where
// every invariant is enforced.
template <size_t N>
struct PartitionTable {
  std::array<PartitionSpec, N> entries;

  [[nodiscard]] constexpr size_t count() const { return N; }

  [[nodiscard]] constexpr const PartitionSpec &operator[](size_t i) const {
    return entries[i];
  }

  // consteval on purpose: a name that is not in the map is a compile error,
  // never a runtime lookup failure. Same idea as gpio()/spi()/exti().
  [[nodiscard]] consteval PartitionSpec find(const char *name) const {
    for (size_t i = 0; i < N; ++i) {
      if (nameEquals(entries[i].name, name)) {
        return entries[i];
      }
    }
    throw "PartitionTable: no partition with this name";
  }
};

// Builds and validates a partition map against a device geometry. Spec may be
// a uniform spec (sensor::W25q32Spec) or a full geometry
// (storage::Stm32f4Geometry<12>) -- GeometryOf sorts that out.
//
// Rejected at compile time: empty or duplicated names, zero size, partitions
// that do not start and end on a sector boundary, partitions that run past the
// device, and any pair that overlaps.
template <typename Spec, size_t N>
consteval PartitionTable<N> partitionTable(const PartitionSpec (&specs)[N]) {
  using Geometry = GeometryOf<Spec>;
  static_assert(
      IFlashGeometry<Geometry>,
      "partitionTable: Spec must be a UniformSpec or model IFlashGeometry"
  );

  for (size_t i = 0; i < N; ++i) {
    const PartitionSpec &p = specs[i];

    if (p.name == nullptr || p.name[0] == '\0') {
      throw "PartitionTable: partition name must not be empty";
    }
    if (p.size == 0) {
      throw "PartitionTable: partition size must be non-zero";
    }
    if (p.offset > Geometry::CAPACITY ||
        p.size > Geometry::CAPACITY - p.offset) {
      throw "PartitionTable: partition does not fit in the device";
    }
    if (!Geometry::isSectorBoundary(p.offset)) {
      throw "PartitionTable: partition offset must be sector-aligned";
    }
    if (!Geometry::isSectorBoundary(p.offset + p.size)) {
      throw "PartitionTable: partition end must be sector-aligned (erasing it "
            "would otherwise touch the next partition)";
    }

    for (size_t j = 0; j < i; ++j) {
      const PartitionSpec &q = specs[j];
      if (nameEquals(q.name, p.name)) {
        throw "PartitionTable: duplicate partition name";
      }
      if (p.offset < q.offset + q.size && q.offset < p.offset + p.size) {
        throw "PartitionTable: partitions overlap";
      }
    }
  }

  PartitionTable<N> table{};
  for (size_t i = 0; i < N; ++i) {
    table.entries[i] = specs[i];
  }
  return table;
}

}  // namespace storage

namespace storage::detail {

struct PartitionProbe {
  static constexpr uint32_t CAPACITY = 8U * 4096U;
  static constexpr uint32_t SECTOR_SIZE = 4096U;
};

consteval bool partitionTableSelfCheck() {
  const PartitionTable table = partitionTable<PartitionProbe>({
      {.name = "boot", .offset = 0, .size = 4096},
      {.name = "config", .offset = 4096, .size = 4096},
      {.name = "storage", .offset = 8192, .size = 8192 + 8192},
  });

  if (table.count() != 3) {
    return false;
  }
  if (table.find("boot").offset != 0 || table.find("boot").size != 4096) {
    return false;
  }
  if (table.find("storage").offset != 8192) {
    return false;
  }
  if (table[1].offset != 4096) {
    return false;
  }

  // Order in the map is free-form: validation is pairwise, not sequential.
  const PartitionTable unordered = partitionTable<PartitionProbe>({
      {.name = "tail", .offset = 4096, .size = 4096},
      {.name = "head", .offset = 0, .size = 4096},
  });
  if (unordered.find("head").offset != 0) {
    return false;
  }

  return nameEquals("abc", "abc") && !nameEquals("abc", "abd") &&
         !nameEquals("abc", "ab") && !nameEquals("ab", "abc");
}

static_assert(partitionTableSelfCheck(), "PartitionTable invariants broken");

}  // namespace storage::detail
