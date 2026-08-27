# FreeRTOS helpers

FreeRTOS is optional: enable `STM32_USE_FREERTOS` in the project `.config`
(the `defconfig` of every template under `templates/freertos/` already sets
it). When enabled, the SDK provides RAII wrappers in
`sdk/rtos/include/rtos/rtos.hpp`.

## Configuration

- Heap implementation: `heap_4`, 16 KB by default — since v0.2.2 the size is
  the Kconfig symbol `FREERTOS_TOTAL_HEAP_SIZE` (default `16384`).
- `configMINIMAL_STACK_SIZE`: 128 words — since v0.2.2 the Kconfig symbol
  `FREERTOS_MINIMAL_STACK_SIZE` (default `128`).
- Tick rate, priority count and the timer-task parameters are Kconfig symbols
  too (`FREERTOS_*`); the defaults reproduce the historic values, see
  [Configuration](../configuration.md).
- `SysTick` is used by FreeRTOS — do not also use `SysTick_Handler` directly.

## `rtos::Task`

Lifetime-managed FreeRTOS task. The instance must outlive the running task —
the typical pattern is a function-static or namespace-scope object after
`vTaskStartScheduler()`:

```cpp
import rtos; // brings in rtos::Task, rtos::Mutex, rtos::Semaphore

void taskFn(void *) {
  while (true) {
    rtos::Task::delay(pdMS_TO_TICKS(100));
  }
}

int main() {
  static rtos::Task t("blink", 256, 1, taskFn);
  rtos::Task::startScheduler();
  while (true) {
  }
}
```

`rtos::Task::delay(ticks)` is the modern wrapper for `vTaskDelay`.

## `rtos::Mutex` / `rtos::Semaphore`

```cpp
rtos::Mutex m;
{
  rtos::MutexGuard guard{m};  // scoped lock
  // critical section
}

rtos::Semaphore sem(0, 1);       // binary semaphore, initial = 0
sem.give();                      // from ISR or task
sem.take();                      // blocking
sem.takeFromIsr(&hpw);           // from ISR
```

Don't call FreeRTOS API **before** `vTaskStartScheduler()` — operations on
mutexes / semaphores hang otherwise. This caught us during the `i2c-scan`
template development; the template was moved to `bare-metal/`
(`STM32_USE_FREERTOS` disabled in its `.config`) to avoid the issue.

## Global object lifetime

The pattern across templates:

```cpp
extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIODEN);
  driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN);
  __DSB();
}

namespace {
GpioPin g_led{*GPIOD, gpio<{.pin = 12, ...}>()};
I2c g_i2c1{*I2C1, {.clockSpeed = 400000, .fastMode = true}};
Uart<> g_uart2{*USART2, USART2_IRQn, {...}};
}  // namespace

int main() {
  static rtos::Task t("worker", 512, 2, workerTask);
  rtos::Task::startScheduler();
  while (true) {
  }
}
```

`__initialize_hardware()` is a weak symbol from SDK; the application overrides
it. It runs **before C++ constructors**, so RCC clocks are enabled in time for
the global peripherals to initialise.

## ISR handlers

Direct references to global objects, not pointers:

```cpp
extern "C" void USART2_IRQHandler() { g_uart2.irqHandler(); }
extern "C" void DMA1_Stream6_IRQHandler() { g_uart2.dmaTxIrqHandler(); }
```

Null-checks on semaphores inside ISR bodies guard against spurious interrupts
firing before the driver has finished construction.
