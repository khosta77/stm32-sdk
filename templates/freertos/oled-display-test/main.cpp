#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"

import driver.types;
import driver.gpio;
import driver.reg;
import driver.stm32f4.gpio;
import driver.stm32f4.i2c;
import sensor.display;
import sensor.ssd1306;

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::gpio;
using driver::GpioConfig;
using driver::OutputSpeed;
using driver::OutputType;
using driver::PinMode;
using driver::PullMode;
using driver::stm32f4::GpioPin;
using driver::stm32f4::I2c;

extern "C" void __initialize_hardware() {
    SystemCoreClockUpdate();
    driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOBEN);
    driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);
    __DSB();
}

namespace {

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

I2c g_i2c1{
    *I2C1,
    {
        .clockSpeed = 400000,
        .fastMode = true,
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

void taskDisplay(void *) {
    rtos::Task::delay(pdMS_TO_TICKS(200));
    if (g_oled.init() != driver::Status::Ok) {
        while (true) {
            rtos::Task::delay(pdMS_TO_TICKS(1000));
        }
    }

    uint32_t counter = 0;
    char buf[32];
    while (true) {
        g_oled.clear();
        g_oled.drawText(0, 0, "STM32 SDK");
        g_oled.drawText(0, 12, "SSD1306 128x64");
        g_oled.drawText(0, 24, "I2C @ 400 kHz");

        snprintf(buf, sizeof(buf), "uptime: %lus", static_cast<unsigned long>(counter));
        g_oled.drawText(0, 48, buf);
        (void) g_oled.flush();

        ++counter;
        rtos::Task::delay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace

int main() {
    static rtos::Task t("display", 512, 1, taskDisplay);

    rtos::Task::startScheduler();

    while (true) {
    }
}
