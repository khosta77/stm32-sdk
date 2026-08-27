#include <cstddef>
#include <cstdint>
#include <cstdio>
// Unlike the other host tests this one declares its own fake flash chips, so
// it needs std::span by name; the modules only #include it in their global
// fragment and therefore do not re-export it.
#include <span>

#include "testing/unit_test.hpp"

import driver.soft_crc;
import driver.types;
import storage.flash_device;
import storage.geometry;
import storage.partition;

using driver::SoftCrc;
using driver::Status;
using storage::ExternalFlashDevice;
using storage::GeometryOf;
using storage::InternalFlashDevice;
using storage::nameEquals;
using storage::Partition;
using storage::PartitionMap;
using storage::Stm32f4Geometry;

namespace {

// Stands in for sensor::W25q32Spec: the same two constants, so the uniform
// path is exercised without pulling the sensor library into the host build.
struct FlashSpec {
  static constexpr uint32_t CAPACITY = 64U * 1024U;
  static constexpr uint32_t SECTOR_SIZE = 4096U;
};

using Map = PartitionMap<
    FlashSpec,
    {.name = "boot", .offset = 0x0000, .size = 0x2000},
    {.name = "config", .offset = 0x2000, .size = 0x1000},
    {.name = "storage", .offset = 0x3000, .size = 0xD000}>;

// Behaves like a real SPI flash chip: programming may not cross a page
// boundary, so the adapter has to split long writes itself.
struct FakeExternalFlash {
  static constexpr uint32_t PAGE = 64;
  static constexpr uint32_t SECTOR = 256;
  static constexpr uint32_t SIZE = 2048;

  uint8_t mem[SIZE];
  size_t pageWrites = 0;

  FakeExternalFlash() {
    for (uint32_t i = 0; i < SIZE; ++i) {
      mem[i] = 0xFF;
    }
  }

  Status read(uint32_t addr, std::span<uint8_t> out) {
    for (size_t i = 0; i < out.size(); ++i) {
      out[i] = mem[addr + i];
    }
    return Status::Ok;
  }

  Status writePage(uint32_t addr, std::span<const uint8_t> in) {
    if (in.size() > PAGE || (addr % PAGE) + in.size() > PAGE) {
      return Status::InvalidArg;
    }
    for (size_t i = 0; i < in.size(); ++i) {
      mem[addr + i] = in[i];
    }
    ++pageWrites;
    return Status::Ok;
  }

  Status eraseSector(uint32_t addr) {
    const uint32_t base = addr - (addr % SECTOR);
    for (uint32_t i = 0; i < SECTOR; ++i) {
      mem[base + i] = 0xFF;
    }
    return Status::Ok;
  }

  [[nodiscard]] uint32_t capacity() const { return SIZE; }
  [[nodiscard]] uint32_t sectorSize() const { return SECTOR; }
  [[nodiscard]] uint32_t pageSize() const { return PAGE; }
};

// The geometry half of the chip, the shape sensor::W25q32Spec has.
struct FakeExternalSpec {
  static constexpr uint32_t CAPACITY = FakeExternalFlash::SIZE;
  static constexpr uint32_t SECTOR_SIZE = FakeExternalFlash::SECTOR;
};

// Models driver::IFlash: absolute addresses, erase by sector index.
struct FakeInternalFlash {
  static constexpr uint32_t SECTOR = 256;
  static constexpr uint8_t SECTORS = 8;
  static constexpr uint32_t SIZE = SECTOR * SECTORS;

  uint8_t mem[SIZE];
  uint32_t base = 0;

  FakeInternalFlash() {
    for (uint32_t i = 0; i < SIZE; ++i) {
      mem[i] = 0xFF;
    }
  }

  Status read(uint32_t addr, std::span<uint8_t> out) {
    for (size_t i = 0; i < out.size(); ++i) {
      out[i] = mem[addr - base + i];
    }
    return Status::Ok;
  }

  Status write(uint32_t addr, std::span<const uint8_t> in) {
    for (size_t i = 0; i < in.size(); ++i) {
      mem[addr - base + i] = in[i];
    }
    return Status::Ok;
  }

  Status eraseSector(uint8_t sector) {
    for (uint32_t i = 0; i < SECTOR; ++i) {
      mem[sector * SECTOR + i] = 0xFF;
    }
    return Status::Ok;
  }

  [[nodiscard]] size_t sectorSize(uint8_t) const { return SECTOR; }
  [[nodiscard]] uint8_t sectorCount() const { return SECTORS; }
};

struct FakeInternalGeometry {
  static constexpr uint32_t SECTOR = FakeInternalFlash::SECTOR;
  static constexpr uint32_t CAPACITY = FakeInternalFlash::SIZE;

  static constexpr bool isSectorBoundary(uint32_t off) {
    return (off % SECTOR) == 0;
  }
  static constexpr uint32_t sectorSizeAt(uint32_t) { return SECTOR; }
  static constexpr uint8_t sectorIndexAt(uint32_t off) {
    return static_cast<uint8_t>(off / SECTOR);
  }
  static constexpr uint32_t sectorBase(uint32_t off) {
    return off - (off % SECTOR);
  }
};

using DeviceMap = PartitionMap<
    FakeInternalGeometry,
    {.name = "head", .offset = 0, .size = 512},
    {.name = "tail", .offset = 512, .size = 1536}>;

void put(const char *s) {
  std::fputs(s, stdout);
}

}  // namespace

TEST(partition_table_keeps_every_entry) {
  EXPECT_EQ(Map::count(), size_t{3});
  EXPECT_EQ(Map::at(0).offset, 0x0000U);
  EXPECT_EQ(Map::at(2).size, 0xD000U);
}

TEST(partition_lookup_by_name_is_compile_time) {
  // find() is consteval, so these are resolved by the compiler; a typo in the
  // name would not build at all.
  constexpr auto boot = Map::find<"boot">();
  constexpr auto storage = Map::find<"storage">();
  EXPECT_EQ(boot.offset, 0x0000U);
  EXPECT_EQ(boot.size, 0x2000U);
  EXPECT_EQ(storage.offset, 0x3000U);
}

TEST(partition_map_covers_the_device_without_gaps_or_overlap) {
  uint32_t covered = 0;
  for (size_t i = 0; i < Map::count(); ++i) {
    covered += Map::at(i).size;
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
  using F4Map = PartitionMap<
      Stm32f4Geometry<12>,
      {.name = "firmware", .offset = 0, .size = 128U * 1024U},
      {.name = "data", .offset = 128U * 1024U, .size = 128U * 1024U}>;
  EXPECT_EQ(F4Map::count(), size_t{2});
  EXPECT_EQ(F4Map::find<"data">().offset, 128U * 1024U);
}

TEST(external_device_splits_long_writes_across_pages) {
  FakeExternalFlash flash;
  ExternalFlashDevice device{flash};

  uint8_t payload[200];
  for (size_t i = 0; i < sizeof(payload); ++i) {
    payload[i] = static_cast<uint8_t>(i);
  }

  // Starts mid-page and runs past three page boundaries: the chip driver
  // would reject this outright, the adapter must slice it up. With PAGE = 64
  // the cuts are 14 + 64 + 64 + 58 bytes.
  ASSERT_EQ(device.write(50, {payload, sizeof(payload)}), Status::Ok);
  EXPECT_EQ(flash.pageWrites, size_t{4});

  uint8_t back[200];
  ASSERT_EQ(device.read(50, {back, sizeof(back)}), Status::Ok);
  for (size_t i = 0; i < sizeof(payload); ++i) {
    EXPECT_EQ(back[i], payload[i]);
  }
  // The byte before the written range must still be erased.
  EXPECT_EQ(flash.mem[49], uint8_t{0xFF});
}

TEST(partition_addresses_relative_to_its_offset) {
  FakeInternalFlash flash;
  InternalFlashDevice<FakeInternalFlash, FakeInternalGeometry> device{flash, 0};
  Partition tail{device, DeviceMap::find<"tail">()};

  const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  ASSERT_EQ(tail.write(0, {data, sizeof(data)}), Status::Ok);

  // Offset 0 of "tail" is byte 512 of the device.
  EXPECT_EQ(flash.mem[512], uint8_t{0xDE});
  EXPECT_EQ(flash.mem[515], uint8_t{0xEF});
  EXPECT_EQ(flash.mem[0], uint8_t{0xFF});

  uint8_t back[4] = {};
  ASSERT_EQ(tail.read(0, {back, sizeof(back)}), Status::Ok);
  EXPECT_EQ(back[0], uint8_t{0xDE});
  EXPECT_EQ(tail.size(), 1536U);
  EXPECT_EQ(tail.deviceOffset(), 512U);
  EXPECT_TRUE(nameEquals(tail.name(), "tail"));
}

TEST(partition_rejects_access_past_its_end) {
  FakeInternalFlash flash;
  InternalFlashDevice<FakeInternalFlash, FakeInternalGeometry> device{flash, 0};
  Partition head{device, DeviceMap::find<"head">()};

  uint8_t buffer[8] = {};
  // "head" is 512 bytes: byte 512 already belongs to "tail".
  EXPECT_EQ(head.read(508, {buffer, sizeof(buffer)}), Status::InvalidArg);
  EXPECT_EQ(head.write(508, {buffer, sizeof(buffer)}), Status::InvalidArg);
  EXPECT_EQ(head.erase(256, 512), Status::InvalidArg);
  // Right up to the edge is fine.
  EXPECT_EQ(head.read(504, {buffer, sizeof(buffer)}), Status::Ok);
}

TEST(partition_erase_leaves_the_neighbour_alone) {
  FakeInternalFlash flash;
  InternalFlashDevice<FakeInternalFlash, FakeInternalGeometry> device{flash, 0};
  Partition head{device, DeviceMap::find<"head">()};
  Partition tail{device, DeviceMap::find<"tail">()};

  const uint8_t marker[] = {0x11, 0x22};
  ASSERT_EQ(head.write(0, {marker, sizeof(marker)}), Status::Ok);
  ASSERT_EQ(tail.write(0, {marker, sizeof(marker)}), Status::Ok);

  ASSERT_EQ(head.erase(), Status::Ok);

  EXPECT_EQ(flash.mem[0], uint8_t{0xFF});
  EXPECT_EQ(flash.mem[511], uint8_t{0xFF});
  // "tail" survives untouched.
  EXPECT_EQ(flash.mem[512], uint8_t{0x11});
  EXPECT_EQ(flash.mem[513], uint8_t{0x22});
}

TEST(partition_checksum_matches_software_crc) {
  FakeInternalFlash flash;
  InternalFlashDevice<FakeInternalFlash, FakeInternalGeometry> device{flash, 0};
  Partition head{device, DeviceMap::find<"head">()};

  uint8_t payload[512];
  for (size_t i = 0; i < sizeof(payload); ++i) {
    payload[i] = static_cast<uint8_t>(i * 7 + 1);
  }
  ASSERT_EQ(head.write(0, {payload, sizeof(payload)}), Status::Ok);

  SoftCrc crc;
  const auto sum = head.checksum(crc);
  ASSERT_TRUE(sum.ok());

  SoftCrc reference;
  // The partition is 512 bytes and the internal chunking is 64, so this also
  // proves the streaming loop stitches chunks together correctly.
  EXPECT_EQ(sum.value(), reference.compute({payload, sizeof(payload)}));
}

TEST(partition_over_external_flash_splits_pages) {
  FakeExternalFlash flash;
  ExternalFlashDevice device{flash};
  using LogMap = PartitionMap<
      FakeExternalSpec,
      {.name = "logs", .offset = 256, .size = 512}>;
  Partition logs{device, LogMap::find<"logs">()};

  uint8_t payload[130];
  for (size_t i = 0; i < sizeof(payload); ++i) {
    payload[i] = static_cast<uint8_t>(0xA0 + (i & 0x0F));
  }
  ASSERT_EQ(logs.write(10, {payload, sizeof(payload)}), Status::Ok);

  // Landed at device offset 266, and nothing before the partition moved.
  EXPECT_EQ(flash.mem[266], uint8_t{0xA0});
  EXPECT_EQ(flash.mem[255], uint8_t{0xFF});
  EXPECT_EQ(logs.erasedValue(), uint8_t{0xFF});
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
  RUN_TEST(runner, external_device_splits_long_writes_across_pages);
  RUN_TEST(runner, partition_addresses_relative_to_its_offset);
  RUN_TEST(runner, partition_rejects_access_past_its_end);
  RUN_TEST(runner, partition_erase_leaves_the_neighbour_alone);
  RUN_TEST(runner, partition_checksum_matches_software_crc);
  RUN_TEST(runner, partition_over_external_flash_splits_pages);
  return runner.summary() ? 0 : 1;
}
