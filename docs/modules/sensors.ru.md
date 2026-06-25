# Интерфейсы сенсоров

Драйверы сенсоров живут в `sdk/sensors/`. С v0.1.7 каждый тип сенсора
описан C++20-**концептом** (`IImu`, `IDisplay`, `IExternalFlash`), а сам
сенсор — **шаблон класса, параметризованный шиной**, например
`template <driver::II2c I2cDriver> class Ssd1306`. Vtable нет: вызовы шины внутри
сенсора прямые. CTAD сохраняет эргономику создания — пишете
`sensor::Ssd1306 oled{i2c, {...}}`, и тип шины выводится из первого аргумента.
Код приложения не меняется при замене одного дисплея на другой, пока оба
удовлетворяют `IDisplay`.

## `IDisplay` — `sensor.display`

```cpp
import sensor.display;

// Любой тип с этими методами удовлетворяет sensor::IDisplay — без наследования.
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

`drawText` намеренно принимает `const char *` (не `std::string_view`) — чтобы
call-сайтам не приходилось `#include <string_view>`. Это ограничение C++20
модулей в этом SDK: header-include в `main.cpp` могут конфликтовать с
`import` модуля, использующего тот же header в global module fragment.

Существующая реализация: `sensor.ssd1306`.

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

Запись — пагинированная, стирание — посекторное. `chipErase()` долгий (секунды),
по возможности используйте посекторное стирание.

Существующая реализация: `sensor.w25q32`
(`template <driver::ISpi SpiDriver, driver::IGpioPin GpioDriver> class W25q32`).
Константы геометрии / идентичности устройства — нешаблонные, в
`sensor::W25q32Spec` (`CAPACITY`, `SECTOR_SIZE`, `PAGE_SIZE`, `JEDEC_W25Q32JV`),
чтобы пользовательский код мог называть их без указания шаблонных аргументов
шины/CS.

## `IImu` — `sensor.imu`

```cpp
import sensor.imu;

// sensor::IImu требует: init, read(ImuData&), selfTest (все -> driver::Status),
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

`ImuData` несёт `accel` / `gyro` (`Vec3f`, в м/с² и °/с) и `temp`.
Существующая реализация: `sensor.mpu6050`
(`template <driver::II2c I2cDriver> class Mpu6050`):

```cpp
sensor::Mpu6050 imu{i2c, {.addr = 0x68, .accelRange = 2, .gyroRange = 250,
                          .sampleRateDiv = 7, .dlpfMode = 6}};  // Mpu6050<I2c> через CTAD
```

## Добавление нового сенсора

1. Выберите контракт — если концепт для типа сенсора уже есть, удовлетворите
   его; иначе создайте `sensor/<тип>.cppm` с новым C++20-концептом. Сделайте
   реализацию шаблоном по типу шины и завершите модуль self-check'ом
   `static_assert(IKind<Impl<MockBus>>)` (тривиальный mock-`struct`,
   удовлетворяющий концепту шины).
2. Создайте `sdk/sensors/<категория>/<имя>/<имя>.cppm` (например,
   `sdk/sensors/displays/ssd1306/ssd1306.cppm`).
3. Подключите модуль в `sdk/cmake/stm32_sensors.cmake`.
4. Добавьте ссылки на даташит и распиновку в `docs/sensors/<имя>.md`
   (EN и RU).
5. Обновите `docs/modules/sensors.md`, если интерфейс поменялся.
