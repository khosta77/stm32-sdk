# Storage — flash partitions

Enabled by `STM32_USE_STORAGE` (requires `STM32_USE_DRIVERS`), linked as
`stm32_storage`. A declarative partition map in the spirit of Zephyr's
`flash_map`, but validated by `consteval` instead of generated from
devicetree: overlaps, unaligned boundaries and out-of-range entries are
compile errors, and a partition is addressed by name.

The layer is CMSIS-free end to end — the adapters talk to driver concepts, not
to registers — so it is fully covered by host tests
(`tests/host/test_partition.cpp`).

## The map — `storage.partition`

```cpp
import sensor.w25q32;
import storage.partition;

using Partitions = storage::PartitionMap<
    sensor::W25q32Spec,
    {.name = "boot",    .offset = 0x000000, .size = 0x010000},
    {.name = "config",  .offset = 0x010000, .size = 0x001000},
    {.name = "storage", .offset = 0x011000, .size = 0x3EF000}>;

constexpr auto kConfig = Partitions::find<"config">();
```

The map is a type, not an object: every entry is a template argument, which is
what lets each invariant be a `static_assert`. Rejected at compile time:

- an empty map, or an empty or duplicated name;
- a name longer than 15 characters (`PARTITION_NAME_MAX` is 16 including the
  NUL);
- a zero size;
- an offset or an end that is not on a sector boundary (erasing such a
  partition would take a bite out of its neighbour);
- a partition running past the end of the device;
- any pair of partitions that overlaps — checked pairwise, so entries need not
  be listed in order.

A violated invariant names itself:

```
error: static assertion failed: PartitionMap: partition offset must be sector-aligned
note: In instantiation of 'struct storage::PartitionMap<..., PartitionSpec{
      PartitionName{"config"}, 65537, 4096}>'
```

`find<"name">()` is `consteval`, so a misspelled name is a build error rather
than a runtime lookup failure. `count()` and `at(i)` are `constexpr` and are
what a runtime loop over the map uses.

Names are stored inline as a fixed 16-byte buffer rather than as a
`const char *`, because a pointer to a string literal is not a valid template
argument. `Partition::name()` still hands out a `const char *` into that
buffer, so `nameEquals` and printf-style callers are unaffected.

The type parameter is the device geometry. `sensor::W25q32Spec` works as-is
because it exposes `CAPACITY` and `SECTOR_SIZE`; the internal flash uses
`storage::Stm32f4Geometry<12>`, which describes the mixed 16K/64K/128K layout
(see below).

## Geometry — `storage.geometry`

Two concepts and two models:

- `UniformSpec` — anything exposing `CAPACITY` + `SECTOR_SIZE`. Wrapped
  automatically by `UniformGeometry`.
- `IFlashGeometry` — the full contract used by the validator: `CAPACITY`,
  `isSectorBoundary(off)`, `sectorSizeAt(off)`.
- `Stm32f4Geometry<SectorCount>` — the F4 internal flash: 4×16K, 1×64K, then
  128K sectors, `CAPACITY` computed from `SectorCount` (1 MB at 12). It
  mirrors `driver::stm32f4::InternalFlash::sectorSize()`, which is a runtime
  method and therefore unusable in a constant expression; a `consteval`
  self-check keeps the two descriptions in step.

`GeometryOf<Spec>` picks the right one, which is why both spellings of
`PartitionMap<...>` work.

## Devices — `storage.flash_device`

`storage::IFlashDevice` is what the partition view needs from a medium: offset
addressing from zero, writes of any length, erase by byte range. Neither
existing flash contract offers that directly, so the layer ships two adapters:

```cpp
import storage.flash_device;

// External SPI flash.
storage::ExternalFlashDevice device{flash};   // flash is a sensor::W25q32

// Internal flash.
driver::stm32f4::InternalFlash raw{12};
storage::InternalFlashDevice<decltype(raw), storage::Stm32f4Geometry<12>>
    device{raw};                              // base defaults to 0x08000000
```

- `ExternalFlashDevice` adds the **multi-page write** the chip driver lacks:
  programming may not cross a 256-byte page boundary, so a long buffer is
  split at every page edge. It states its requirements structurally
  (`ExternalFlashLike`) rather than importing `sensor.external_flash`, so the
  storage layer does not pull in `STM32_USE_SENSORS`.
- `InternalFlashDevice` translates an offset into the absolute address
  `driver::IFlash` expects, and a byte range into the sector indices its
  `eraseSector(uint8_t)` takes.

## The partition view

```cpp
storage::Partition config{device, kConfig};

(void) config.erase();                        // the whole partition
(void) config.write(0, {data, len});          // offset 0 == start of "config"
(void) config.read(0, {buffer, len});

driver::stm32f4::Crc crc{*CRC};
const auto sum = config.checksum(crc);        // driver::Result<uint32_t>
```

- Offsets are **relative to the partition**, exactly like Zephyr's
  `flash_area_read(fa, off, ...)`.
- Anything reaching past the end returns `Status::InvalidArg` instead of
  spilling into the neighbouring partition.
- `checksum()` streams the partition through any `driver::ICrc` engine in
  64-byte chunks — no staging buffer, which is why `ICrc` keeps
  `reset`/`update`/`value` separate. Works with the hardware `Crc` and with
  `SoftCrc` alike; both produce CRC-32/IEEE, so the value can be reproduced on
  a workstation with `zlib.crc32` over a dump of the same range.
- `size()`, `deviceOffset()`, `name()` and `erasedValue()` describe the
  partition itself.

## Notes

- **No devicetree.** Zephyr keeps `flash_map` in DTS because C99 cannot
  compute at compile time and needs an external declarative format plus a
  generator with bindings. C++20 `consteval` gives the same guarantee
  type-safely, with no extra tooling.
- Partition names live in flash as plain literals; lookup happens entirely at
  compile time, so nothing searches strings at runtime.
- The layer does not modify `W25q32` or `InternalFlash` — bounds checking and
  page splitting live in the adapters, so existing behaviour is unchanged.
- A partition header (magic / version / length / CRC) is deliberately out of
  scope; that is a data format, not a map.

Demo: the `freertos/w25q32-flash-test` template writes through a partition,
proves the neighbour is untouched after a full erase, and cross-checks the
hardware CRC against the software one.
