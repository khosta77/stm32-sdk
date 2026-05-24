#include "cmsis/stm32f4xx.h"

static volatile uint32_t g_ticks;

extern "C" void SysTick_Handler() {
    g_ticks = g_ticks + 1;
}

static void delay(uint32_t ms) {
    uint32_t start = g_ticks;
    while (g_ticks - start < ms) {
    }
}

constexpr uint32_t LED_GREEN = GPIO_ODR_OD12;
constexpr uint32_t LED_ORANGE = GPIO_ODR_OD13;
constexpr uint32_t LED_RED = GPIO_ODR_OD14;
constexpr uint32_t LED_BLUE = GPIO_ODR_OD15;
constexpr uint32_t LED_ALL = LED_GREEN | LED_ORANGE | LED_RED | LED_BLUE;

int main() {
    SysTick_Config(SystemCoreClock / 1000);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    __DSB();

    GPIOD->MODER |= GPIO_MODER_MODER12_0 | GPIO_MODER_MODER13_0 | GPIO_MODER_MODER14_0 | GPIO_MODER_MODER15_0;

    while (true) {
        GPIOD->BSRR = LED_GREEN;
        delay(125);
        GPIOD->BSRR = LED_ORANGE;
        delay(125);
        GPIOD->BSRR = LED_RED;
        delay(125);
        GPIOD->BSRR = LED_BLUE;
        delay(125);

        GPIOD->BSRR = LED_ALL << 16;
        delay(500);
    }
}
