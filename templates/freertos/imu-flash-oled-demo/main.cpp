#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"

import driver.types;
import driver.gpio;
import driver.i2c;
import driver.spi;
import driver.reg;
import driver.uart;
import driver.stm32f4.gpio;
import driver.stm32f4.i2c;
import driver.stm32f4.spi;
import driver.stm32f4.uart;
import sensor.imu;
import sensor.mpu6050;
import sensor.display;
import sensor.ssd1306;
import sensor.external_flash;
import sensor.w25q32;
import system.component;
import system.bootstrap;

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::DataBits;
using driver::gpio;
using driver::i2c;
using driver::OutputSpeed;
using driver::OutputType;
using driver::Parity;
using driver::PinMode;
using driver::PullMode;
using driver::spi;
using driver::SpiDataSize;
using driver::SpiMode;
using driver::StopBits;
using driver::uart;
using driver::stm32f4::GpioPin;
using driver::stm32f4::I2c;
using driver::stm32f4::Spi;
using driver::stm32f4::Uart;

extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(
      RCC->AHB1ENR,
      RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN
  );
  driver::reg::set(
      RCC->APB1ENR,
      RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN | RCC_APB1ENR_SPI2EN
  );
  __DSB();
}

namespace {

template <driver::II2c I2cDriver>
class ImuSampler : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
    uint32_t periodMs;
  };
  struct Environment {
    I2cDriver &i2c;
  };

  ImuSampler(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _periodMs(cfg.periodMs),
        _mpu(
            env.i2c,
            {
                .addr = 0x68,
                .accelRange = 2,
                .gyroRange = 250,
                .sampleRateDiv = 7,
                .dlpfMode = 6,
            }
        ) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return _mpu.init(); }
  driver::Status onBind() { return driver::Status::Ok; }
  driver::Status onStart() { return driver::Status::Ok; }

  void run() {
    while (true) {
      sensor::ImuData sample;
      if (_mpu.read(sample) == driver::Status::Ok) {
        rtos::LockGuard lock(_mtx);
        _latest = sample;
      }
      rtos::Task::delay(pdMS_TO_TICKS(_periodMs));
    }
  }

  [[nodiscard]] sensor::ImuData latest() {
    rtos::LockGuard lock(_mtx);
    return _latest;
  }

private:
  uint32_t _periodMs;
  sensor::Mpu6050<I2cDriver> _mpu;
  rtos::Mutex _mtx;
  sensor::ImuData _latest{};
};

template <driver::II2c I2cDriver>
class DisplayView : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
    uint32_t periodMs;
  };
  struct Environment {
    I2cDriver &i2c;
    ImuSampler<I2cDriver> &imu;
  };

  DisplayView(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _periodMs(cfg.periodMs),
        _imu(env.imu),
        _oled(
            env.i2c,
            {
                .addr = 0x3C,
                .contrast = 0x7F,
                .flipH = false,
                .flipV = false,
            }
        ) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return _oled.init(); }
  driver::Status onBind() { return driver::Status::Ok; }
  driver::Status onStart() { return driver::Status::Ok; }

  void run() {
    char line[32];
    while (true) {
      const sensor::ImuData data = _imu.latest();
      const int ax = static_cast<int>(data.accel.x * 100);
      const int ay = static_cast<int>(data.accel.y * 100);
      const int az = static_cast<int>(data.accel.z * 100);
      const int t = static_cast<int>(data.temp * 10);

      _oled.clear();
      _oled.drawText(0, 0, "MPU-6050 cm/s2");
      snprintf(line, sizeof(line), "ax %+5d", ax);
      _oled.drawText(0, 16, line);
      snprintf(line, sizeof(line), "ay %+5d", ay);
      _oled.drawText(0, 26, line);
      snprintf(line, sizeof(line), "az %+5d", az);
      _oled.drawText(0, 36, line);
      snprintf(
          line,
          sizeof(line),
          "T %d.%dC",
          t / 10,
          t % 10 < 0 ? -t % 10 : t % 10
      );
      _oled.drawText(0, 52, line);

      (void) _oled.flush();
      rtos::Task::delay(pdMS_TO_TICKS(_periodMs));
    }
  }

private:
  uint32_t _periodMs;
  ImuSampler<I2cDriver> &_imu;
  sensor::Ssd1306<I2cDriver> _oled;
};

template <
    driver::ISpi SpiDriver,
    driver::IGpioPin CsPin,
    driver::II2c I2cDriver>
class FlashLogger : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
    uint32_t periodMs;
  };
  struct Environment {
    SpiDriver &spi;
    CsPin &cs;
    ImuSampler<I2cDriver> &imu;
  };

  FlashLogger(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _periodMs(cfg.periodMs),
        _imu(env.imu),
        _flash(
            env.spi,
            env.cs,
            {
                .expectedJedecId = sensor::W25q32Spec::JEDEC_W25Q32JV,
                .busyPollLoops = 5000000U,
            }
        ) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() {
    const driver::Status st = _flash.init();
    if (st != driver::Status::Ok) {
      return st;
    }
    return _flash.eraseSector(0);
  }
  driver::Status onBind() { return driver::Status::Ok; }
  driver::Status onStart() { return driver::Status::Ok; }

  void run() {
    uint32_t offset = 0;
    char buf[32];
    while (true) {
      rtos::Task::delay(pdMS_TO_TICKS(_periodMs));
      if (offset + sizeof(buf) > sensor::W25q32Spec::SECTOR_SIZE) {
        offset = 0;
        (void) _flash.eraseSector(0);
      }
      const sensor::ImuData data = _imu.latest();
      const int ax = static_cast<int>(data.accel.x * 100);
      const int ay = static_cast<int>(data.accel.y * 100);
      const int az = static_cast<int>(data.accel.z * 100);
      const int len =
          snprintf(buf, sizeof(buf), "%+5d %+5d %+5d\n", ax, ay, az);
      if (len > 0) {
        (void) _flash.writePage(
            offset,
            {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
        );
        offset += static_cast<uint32_t>(len);
      }
    }
  }

private:
  uint32_t _periodMs;
  ImuSampler<I2cDriver> &_imu;
  sensor::W25q32<SpiDriver, CsPin> _flash;
};

struct DemoApp {
  GpioPin uartTx{
      *GPIOA,
      gpio<{
          .pin = 2,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 7,
      }>(),
  };
  GpioPin uartRx{
      *GPIOA,
      gpio<{
          .pin = 3,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 7,
      }>(),
  };
  GpioPin i2cScl{
      *GPIOB,
      gpio<{
          .pin = 6,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::PullUp,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::OpenDrain,
          .af = 4,
      }>(),
  };
  GpioPin i2cSda{
      *GPIOB,
      gpio<{
          .pin = 7,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::PullUp,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::OpenDrain,
          .af = 4,
      }>(),
  };
  GpioPin spiSck{
      *GPIOB,
      gpio<{
          .pin = 13,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 5,
      }>(),
  };
  GpioPin spiMiso{
      *GPIOB,
      gpio<{
          .pin = 14,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 5,
      }>(),
  };
  GpioPin spiMosi{
      *GPIOB,
      gpio<{
          .pin = 15,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 5,
      }>(),
  };
  GpioPin flashCs{
      *GPIOB,
      gpio<{
          .pin = 12,
          .mode = PinMode::Output,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
      }>(),
  };

  I2c i2c1{
      *I2C1,
      i2c<{
          .clockSpeed = 400000,
          .fastMode = true,
      }>(),
  };
  Spi spi2{
      *SPI2,
      spi<{
          .clockHz = 10000000,
          .mode = SpiMode::Mode0,
          .lsbFirst = false,
          .dataSize = SpiDataSize::Bits8,
      }>(),
  };
  Uart<> uart2{
      *USART2,
      USART2_IRQn,
      uart<{
          .baudrate = 115200,
          .dataBits = DataBits::Eight,
          .stopBits = StopBits::One,
          .parity = Parity::None,
      }>(),
  };

  ImuSampler<I2c> imu{
      {
          .base = {.name = "imu", .criticality = system::Criticality::Critical},
          .periodMs = 20,
      },
      {.i2c = i2c1},
  };
  DisplayView<I2c> view{
      {
          .base = {.name = "oled", .criticality = system::Criticality::Common},
          .periodMs = 200,
      },
      {.i2c = i2c1, .imu = imu},
  };
  FlashLogger<Spi, GpioPin, I2c> logger{
      {
          .base = {.name = "flash", .criticality = system::Criticality::Common},
          .periodMs = 5000,
      },
      {.spi = spi2, .cs = flashCs, .imu = imu},
  };

  [[nodiscard]] system::BootReport boot() {
    return system::bootstrap(imu, view, logger);
  }
};

static_assert(system::Component<ImuSampler<I2c>>);
static_assert(system::Component<DisplayView<I2c>>);
static_assert(system::Component<FlashLogger<Spi, GpioPin, I2c>>);

DemoApp app;

void writeStr(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') {
    ++n;
  }
  (void) app.uart2.write({reinterpret_cast<const uint8_t *>(s), n});
}

void writePrintf(const char *fmt, auto... args) {
  char buf[96];
  const int len = snprintf(buf, sizeof(buf), fmt, args...);
  if (len > 0) {
    (void) app.uart2.write(
        {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
    );
  }
}

void imuEntry(void *p) {
  static_cast<decltype(&app.imu)>(p)->run();
}
void viewEntry(void *p) {
  static_cast<decltype(&app.view)>(p)->run();
}
void loggerEntry(void *p) {
  static_cast<decltype(&app.logger)>(p)->run();
}

}  // namespace

extern "C" void USART2_IRQHandler() {
  app.uart2.irqHandler();
}

int main() {
  const system::BootReport report = app.boot();

  writeStr("\r\n=== imu-flash-oled-demo bootstrap ===\r\n");
  if (report.status != driver::Status::Ok) {
    writePrintf(
        "CRITICAL: %s failed (status=%d, phase=%d)\r\n",
        report.failedComponent,
        static_cast<int>(report.status),
        static_cast<int>(report.failedPhase)
    );
  } else if (report.degraded > 0) {
    writePrintf(
        "degraded: %u component(s) failed, continuing\r\n",
        report.degraded
    );
  } else {
    writeStr("all components started\r\n");
  }

  static rtos::Task tImu("imu", 384, 2, imuEntry, &app.imu);
  static rtos::Task tView("view", 512, 1, viewEntry, &app.view);
  static rtos::Task tLogger("logger", 512, 1, loggerEntry, &app.logger);

  rtos::Task::startScheduler();

  while (true) {
  }
}
