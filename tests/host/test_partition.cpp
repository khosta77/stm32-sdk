#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "testing/unit_test.hpp"

import storage.geometry;
import storage.partition;

using storage::GeometryOf;
using storage::nameEquals;
using storage::partitionTable;
using storage::Stm32f4Geometry;

namespace {

// Stands in for sensor::W25q32Spec: the same two constants, so the uniform
// path is exercised without pulling the sensor library into the host build.
struct FlashSpec {
  static constexpr uint32_t CAPACITY = 64U * 1024U;
  static constexpr uint32_t SECTOR_SIZE = 4096U;
};

constexpr auto kMap = partitionTable<FlashSpec>({
    {.name = "boot", .offset = 0x0000, .size = 0x2000},
    {.name = "config", .offset = 0x2000, .size = 0x1000},
    {.name = "storage", .offset = 0x3000, .size = 0xD000},
});

void put(const char *s) {
  std::fputs(s, stdout);
}

}  // namespace

TEST(partition_table_keeps_every_entry) {
  EXPECT_EQ(kMap.count(), size_t{3});
  EXPECT_EQ(kMap[0].offset, 0x0000U);
  EXPECT_EQ(kMap[2].size, 0xD000U);
}

TEST(partition_lookup_by_name_is_compile_time) {
  // find() is consteval, so these are resolved by the compiler; a typo in the
  // name would not build at all.
  constexpr auto boot = kMap.find("boot");
  constexpr auto storage = kMap.find("storage");
  EXPECT_EQ(boot.offset, 0x0000U);
  EXPECT_EQ(boot.size, 0x2000U);
  EXPECT_EQ(storage.offset, 0x3000U);
}

TEST(partition_map_covers_the_device_without_gaps_or_overlap) {
  uint32_t covered = 0;
  for (size_t i = 0; i < kMap.count(); ++i) {
    covered += kMap[i].size;
  }
  EXPECT_EQ(covered, FlashSpec::CAPACITY);
}

TEST(name_comparison_handles_prefixes_and_null) {
  EXPECT_TRUE(nameEquals("storage", "storage"));
  EXPECT_FALSE(nameEquals("storage", "storag"));
  EXPECT_FALSE(nameEquals("storag", "storage"));
  EXPECT_FALSE(nameEquals("boot", "config"));
  EXPECT_TRUE(nameEquals(nullptr, nullptr));
  EXPECT_FALSE(nameEquals(nullptr, "boot"));
}

TEST(uniform_geometry_is_derived_from_a_spec) {
  using Geometry = GeometryOf<FlashSpec>;
  EXPECT_EQ(Geometry::CAPACITY, 64U * 1024U);
  EXPECT_TRUE(Geometry::isSectorBoundary(0));
  EXPECT_TRUE(Geometry::isSectorBoundary(4096U));
  EXPECT_FALSE(Geometry::isSectorBoundary(4095U));
  EXPECT_EQ(Geometry::sectorSizeAt(9000U), 4096U);
}

TEST(f4_geometry_matches_the_internal_flash_layout) {
  using F4 = Stm32f4Geometry<12>;

  // Mirrors driver::stm32f4::InternalFlash::sectorSize(): 4x16K, 1x64K, 7x128K.
  for (uint8_t sector = 0; sector < 4; ++sector) {
    EXPECT_EQ(F4::sectorSize(sector), 16U * 1024U);
  }
  EXPECT_EQ(F4::sectorSize(4), 64U * 1024U);
  for (uint8_t sector = 5; sector < 12; ++sector) {
    EXPECT_EQ(F4::sectorSize(sector), 128U * 1024U);
  }

  EXPECT_EQ(F4::CAPACITY, 1024U * 1024U);
  EXPECT_TRUE(F4::isSectorBoundary(F4::CAPACITY));
  EXPECT_FALSE(F4::isSectorBoundary(1U));
  EXPECT_EQ(F4::sectorIndexAt(64U * 1024U), uint8_t{4});
  EXPECT_EQ(F4::sectorBase(200U * 1024U), 128U * 1024U);
}

TEST(f4_geometry_accepts_a_partition_map) {
  // Sector 4 spans 64K..128K, so a partition may start at 128K but not at 96K.
  constexpr auto map = partitionTable<Stm32f4Geometry<12>>({
      {.name = "firmware", .offset = 0, .size = 128U * 1024U},
      {.name = "data", .offset = 128U * 1024U, .size = 128U * 1024U},
  });
  EXPECT_EQ(map.count(), size_t{2});
  EXPECT_EQ(map.find("data").offset, 128U * 1024U);
}

int main() {
  testing::TestRunner runner{put};
  RUN_TEST(runner, partition_table_keeps_every_entry);
  RUN_TEST(runner, partition_lookup_by_name_is_compile_time);
  RUN_TEST(runner, partition_map_covers_the_device_without_gaps_or_overlap);
  RUN_TEST(runner, name_comparison_handles_prefixes_and_null);
  RUN_TEST(runner, uniform_geometry_is_derived_from_a_spec);
  RUN_TEST(runner, f4_geometry_matches_the_internal_flash_layout);
  RUN_TEST(runner, f4_geometry_accepts_a_partition_map);
  return runner.summary() ? 0 : 1;
}
