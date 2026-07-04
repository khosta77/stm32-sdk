#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"
#include "sensor/cached_sensor.hpp"

import driver.types;
import driver.gpio;
import driver.i2c;
import driver.reg;
import driver.uart;
import driver.stm32f4.dma;
import driver.stm32f4.gpio;
import driver.stm32f4.i2c;
import driver.stm32f4.uart;
import sensor.imu;
import sensor.mpu6050;

extern "C" {
float atan2f(float y, float x);
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::DataBits;
using driver::gpio;
using driver::GpioConfig;
using driver::i2c;
using driver::OutputSpeed;
using driver::OutputType;
using driver::Parity;
using driver::PinMode;
using driver::PullMode;
using driver::StopBits;
using driver::uart;
using driver::stm32f4::GpioPin;
using driver::stm32f4::I2c;
using driver::stm32f4::Uart;
using driver::stm32f4::UartMode;
using driver::stm32f4::dmaMap::usart2_tx;

extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(
      RCC->AHB1ENR,
      RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN |
          RCC_AHB1ENR_DMA1EN
  );
  driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN);
  __DSB();
}

constexpr float TILT_THRESHOLD = 15.0f;
constexpr float RAD_TO_DEG = 180.0f / 3.14159265f;

namespace {

GpioPin g_uartTx{
    *GPIOA,
    gpio({
        .pin = 2,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 7,
    }),
};
GpioPin g_uartRx{
    *GPIOA,
    gpio({
        .pin = 3,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 7,
    }),
};
GpioPin g_i2cScl{
    *GPIOB,
    gpio({
        .pin = 6,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::PullUp,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::OpenDrain,
        .af = 4,
    }),
};
GpioPin g_i2cSda{
    *GPIOB,
    gpio({
        .pin = 7,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::PullUp,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::OpenDrain,
        .af = 4,
    }),
};
GpioPin g_ledGreen{
    *GPIOD,
    gpio({
        .pin = 12,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
GpioPin g_ledOrange{
    *GPIOD,
    gpio({
        .pin = 13,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
GpioPin g_ledRed{
    *GPIOD,
    gpio({
        .pin = 14,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};
GpioPin g_ledBlue{
    *GPIOD,
    gpio({
        .pin = 15,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::Low,
        .type = OutputType::PushPull,
    }),
};

I2c g_i2c1{
    *I2C1,
    i2c({
        .clockSpeed = 400000,
        .fastMode = true,
    }),
};
Uart<256, 256, UartMode::Dma> g_uart2{
    *USART2,
    USART2_IRQn,
    usart2_tx,
    uart({
        .baudrate = 115200,
        .dataBits = DataBits::Eight,
        .stopBits = StopBits::One,
        .parity = Parity::None,
    }),
};
sensor::Mpu6050 g_mpu{
    g_i2c1,
    {
        .addr = 0x68,
        .accelRange = 2,
        .gyroRange = 250,
        .sampleRateDiv = 7,
        .dlpfMode = 6,
    },
};

driver::Status readImu(sensor::ImuData &out, void *ctx) {
  auto *mpu = static_cast<decltype(&g_mpu)>(ctx);
  return mpu->read(out);
}

void taskView(void *param) {
  auto *cached = static_cast<sensor::CachedSensor<sensor::ImuData> *>(param);
  char buf[128];
  while (true) {
    auto data = cached->get();
    int ax = static_cast<int>(data.accel.x * 100);
    int ay = static_cast<int>(data.accel.y * 100);
    int az = static_cast<int>(data.accel.z * 100);
    int gx = static_cast<int>(data.gyro.x * 10);
    int gy = static_cast<int>(data.gyro.y * 10);
    int gz = static_cast<int>(data.gyro.z * 10);
    int t = static_cast<int>(data.temp * 10);
    int len = snprintf(
        buf,
        sizeof(buf),
        "A:%d %d %d G:%d %d %d T:%d\r\n",
        ax,
        ay,
        az,
        gx,
        gy,
        gz,
        t
    );
    if (len > 0) {
      (void) g_uart2.write(
          {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
      );
    }
    rtos::Task::delay(pdMS_TO_TICKS(100));
  }
}

void taskLed(void *param) {
  auto *cached = static_cast<sensor::CachedSensor<sensor::ImuData> *>(param);
  while (true) {
    auto data = cached->get();
    float pitch = atan2f(data.accel.y, data.accel.z) * RAD_TO_DEG;
    float roll = atan2f(data.accel.x, data.accel.z) * RAD_TO_DEG;

    g_ledGreen.reset();
    g_ledOrange.reset();
    g_ledRed.reset();
    g_ledBlue.reset();

    if (pitch > TILT_THRESHOLD) {
      g_ledGreen.set();
    } else if (pitch < -TILT_THRESHOLD) {
      g_ledRed.set();
    }

    if (roll > TILT_THRESHOLD) {
      g_ledOrange.set();
    } else if (roll < -TILT_THRESHOLD) {
      g_ledBlue.set();
    }

    rtos::Task::delay(pdMS_TO_TICKS(50));
  }
}

}  // namespace

extern "C" void USART2_IRQHandler() {
  g_uart2.irqHandler();
}

extern "C" void DMA1_Stream6_IRQHandler() {
  g_uart2.dmaTxIrqHandler();
}

int main() {
  if (g_mpu.init() != driver::Status::Ok) {
    while (true) {
    }  // MPU init failed — halt
  }

  static sensor::CachedSensor<sensor::ImuData>
      cachedImu(10, readImu, &g_mpu, "imu", 256, 2);

  static rtos::Task view("view", 384, 1, taskView, &cachedImu);
  static rtos::Task led("led", 384, 1, taskLed, &cachedImu);

  rtos::Task::startScheduler();

  while (true) {
  }
}
