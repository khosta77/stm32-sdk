#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"
#include "sensor/cached_sensor.hpp"

import driver.types;
import driver.gpio;
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

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::gpio;
using driver::GpioConfig;
using driver::OutputSpeed;
using driver::OutputType;
using driver::Parity;
using driver::PinMode;
using driver::PullMode;
using driver::stm32f4::GpioPin;
using driver::stm32f4::I2c;
using driver::stm32f4::Spi;
using driver::stm32f4::Uart;

extern "C" void __initialize_hardware() {
    SystemCoreClockUpdate();
    driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN);
    driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN | RCC_APB1ENR_SPI2EN);
    __DSB();
}

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
GpioPin g_spiSck{
    *GPIOB,
    gpio({
        .pin = 13,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 5,
    }),
};
GpioPin g_spiMiso{
    *GPIOB,
    gpio({
        .pin = 14,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 5,
    }),
};
GpioPin g_spiMosi{
    *GPIOB,
    gpio({
        .pin = 15,
        .mode = PinMode::AlternateFunction,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
        .af = 5,
    }),
};
GpioPin g_flashCs{
    *GPIOB,
    gpio({
        .pin = 12,
        .mode = PinMode::Output,
        .pull = PullMode::None,
        .speed = OutputSpeed::VeryHigh,
        .type = OutputType::PushPull,
    }),
};

I2c g_i2c1{
    *I2C1,
    {
        .clockSpeed = 400000,
        .fastMode = true,
    },
};
Spi g_spi2{
    *SPI2,
    {
        .clockHz = 10000000,
        .mode = 0,
        .lsbFirst = false,
        .dataSize = 8,
    },
};
Uart<> g_uart2{
    *USART2,
    USART2_IRQn,
    {
        .baudrate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = Parity::None,
    },
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
sensor::Ssd1306 g_oled{
    g_i2c1,
    {
        .addr = 0x3C,
        .contrast = 0x7F,
        .flipH = false,
        .flipV = false,
    },
};
sensor::W25q32 g_flash{
    g_spi2,
    g_flashCs,
    {
        .expectedJedecId = sensor::W25q32Spec::JEDEC_W25Q32JV,
        .busyPollLoops = 5000000U,
    },
};

driver::Status readImu(sensor::ImuData &out, void *ctx) {
    auto *mpu = static_cast<decltype(&g_mpu)>(ctx);
    return mpu->read(out);
}

void writeStr(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    (void) g_uart2.write({reinterpret_cast<const uint8_t *>(s), n});
}

void taskDisplay(void *param) {
    auto *cached = static_cast<sensor::CachedSensor<sensor::ImuData> *>(param);
    if (g_oled.init() != driver::Status::Ok) {
        writeStr("OLED init failed\r\n");
        while (true) {
            rtos::Task::delay(pdMS_TO_TICKS(1000));
        }
    }
    char line[32];
    while (true) {
        const auto data = cached->get();
        const int ax = static_cast<int>(data.accel.x * 100);
        const int ay = static_cast<int>(data.accel.y * 100);
        const int az = static_cast<int>(data.accel.z * 100);
        const int t = static_cast<int>(data.temp * 10);

        g_oled.clear();
        g_oled.drawText(0, 0, "MPU-6050 cm/s2");
        snprintf(line, sizeof(line), "ax %+5d", ax);
        g_oled.drawText(0, 16, line);
        snprintf(line, sizeof(line), "ay %+5d", ay);
        g_oled.drawText(0, 26, line);
        snprintf(line, sizeof(line), "az %+5d", az);
        g_oled.drawText(0, 36, line);
        snprintf(line, sizeof(line), "T %d.%dC", t / 10, t % 10 < 0 ? -t % 10 : t % 10);
        g_oled.drawText(0, 52, line);

        (void) g_oled.flush();
        rtos::Task::delay(pdMS_TO_TICKS(200));
    }
}

void taskFlashLogger(void *param) {
    auto *cached = static_cast<sensor::CachedSensor<sensor::ImuData> *>(param);
    rtos::Task::delay(pdMS_TO_TICKS(500));
    if (g_flash.init() != driver::Status::Ok) {
        writeStr("flash init failed (check JEDEC)\r\n");
        while (true) {
            rtos::Task::delay(pdMS_TO_TICKS(1000));
        }
    }
    writeStr("flash ready, sector 0 reset\r\n");
    if (g_flash.eraseSector(0) != driver::Status::Ok) {
        writeStr("flash erase failed\r\n");
        while (true) {
            rtos::Task::delay(pdMS_TO_TICKS(1000));
        }
    }

    uint32_t offset = 0;
    char buf[32];
    while (true) {
        rtos::Task::delay(pdMS_TO_TICKS(5000));
        if (offset + sizeof(buf) > sensor::W25q32Spec::SECTOR_SIZE) {
            offset = 0;
            if (g_flash.eraseSector(0) != driver::Status::Ok) {
                writeStr("flash re-erase failed\r\n");
            }
        }
        const auto data = cached->get();
        const int ax = static_cast<int>(data.accel.x * 100);
        const int ay = static_cast<int>(data.accel.y * 100);
        const int az = static_cast<int>(data.accel.z * 100);
        const int len = snprintf(buf, sizeof(buf), "%+5d %+5d %+5d\n", ax, ay, az);
        if (len > 0) {
            if (g_flash.writePage(offset, {reinterpret_cast<const uint8_t *>(buf),
                                           static_cast<size_t>(len)}) != driver::Status::Ok) {
                writeStr("flash write failed\r\n");
            }
            offset += static_cast<uint32_t>(len);
        }
    }
}

}  // namespace

extern "C" void USART2_IRQHandler() {
    g_uart2.irqHandler();
}

int main() {
    if (g_mpu.init() != driver::Status::Ok) {
        writeStr("MPU init failed\r\n");
    }

    static sensor::CachedSensor<sensor::ImuData> cachedImu(50, readImu, &g_mpu, "imu", 384, 2);

    static rtos::Task display("display", 512, 1, taskDisplay, &cachedImu);
    static rtos::Task logger("logger", 512, 1, taskFlashLogger, &cachedImu);

    rtos::Task::startScheduler();

    while (true) {
    }
}
