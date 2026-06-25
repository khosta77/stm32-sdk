# Sensor interfaces

Sensor drivers live under `sdk/sensors/`. Since v0.1.7 each sensor kind is
described by a C++20 **concept** (`IImu`, `IDisplay`, `IExternalFlash`), and a
sensor is a **class template parameterised on its bus** — for example
`template <driver::II2c I2cDriver> class Ssd1306`. There is no vtable: bus calls
inside the sensor are direct. CTAD keeps construction ergonomic — you write
`sensor::Ssd1306 oled{i2c, {...}}` and the bus type is deduced from the first
argument. Your application code stays the same when you swap an SSD1306 for a
different display, as long as both model `IDisplay`.

## `IDisplay` — `sensor.display`

```cpp
import sensor.display;

// Any type that provides these methods models sensor::IDisplay — no inheritance.
struct MyDisplay {
    driver::Status init();
    void clear();
    void setPixel(uint16_t x, uint16_t y, bool on);
    void drawChar(uint16_t x, uint16_t y, char c);
    void drawText(uint16_t x, uint16_t y, const char *text);
    driver::Status flush();
    uint16_t width() const;
    uint16_t height() const;
};
static_assert(sensor::IDisplay<MyDisplay>);
```

`drawText` is intentionally `const char *` (not `std::string_view`) so call
sites don't need `#include <string_view>`. That's a C++20 modules constraint
on this SDK: header includes in `main.cpp` can collide with `import` of a
module that uses the same header in its global module fragment.

Existing implementation: `sensor.ssd1306`.

## `IExternalFlash` — `sensor.external_flash`

```cpp
import sensor.external_flash;

struct MyFlash {
    driver::Status init();
    driver::Status read(uint32_t addr, std::span<uint8_t> data);
    driver::Status writePage(uint32_t addr, std::span<const uint8_t> data);
    driver::Status eraseSector(uint32_t addr);
    driver::Status chipErase();
    uint32_t capacity() const;
    uint32_t sectorSize() const;
    uint32_t pageSize() const;
    uint32_t jedecId() const;
};
static_assert(sensor::IExternalFlash<MyFlash>);
```

Page-aligned writes, sector-aligned erases. `chipErase()` is slow (seconds);
prefer per-sector erase when possible.

Existing implementation: `sensor.w25q32`
(`template <driver::ISpi SpiDriver, driver::IGpioPin GpioDriver> class W25q32`).
Device geometry / identity constants are non-template, in `sensor::W25q32Spec`
(`CAPACITY`, `SECTOR_SIZE`, `PAGE_SIZE`, `JEDEC_W25Q32JV`), so user code can name
them without spelling out the bus/CS template arguments.

## `IImu` — `sensor.imu`

```cpp
import sensor.imu;

// sensor::IImu requires: init, read(ImuData&), selfTest (all -> driver::Status),
// setAccelRange(uint8_t), setGyroRange(uint16_t) (-> void).
struct MyImu {
    driver::Status init();
    driver::Status read(sensor::ImuData &out);
    driver::Status selfTest();
    void setAccelRange(uint8_t g);
    void setGyroRange(uint16_t dps);
};
static_assert(sensor::IImu<MyImu>);
```

`ImuData` carries `accel` / `gyro` (`Vec3f`, in m/s² and °/s) and `temp`.
Existing implementation: `sensor.mpu6050`
(`template <driver::II2c I2cDriver> class Mpu6050`):

```cpp
sensor::Mpu6050 imu{i2c, {.addr = 0x68, .accelRange = 2, .gyroRange = 250,
                          .sampleRateDiv = 7, .dlpfMode = 6}};  // Mpu6050<I2c> via CTAD
```

## Adding a new sensor

1. Decide the contract — if a concept exists for your sensor kind, model it;
   otherwise define `sensor/<kind>.cppm` with a new C++20 concept. Make the
   implementation a template on its bus type and end the module with a
   `static_assert(IKind<Impl<MockBus>>)` self-check (a trivial mock `struct`
   that models the bus concept).
2. Create `sdk/sensors/<category>/<name>/<name>.cppm` (e.g.
   `sdk/sensors/displays/ssd1306/ssd1306.cppm`).
3. Wire the module into `sdk/cmake/stm32_sensors.cmake`.
4. Add datasheet links and pinout in `docs/sensors/<name>.md` (EN and RU).
5. Bump `docs/modules/sensors.md` if the interface changed.
