# FreeRTOS-обёртки

FreeRTOS подключается опционально: включите `STM32_USE_FREERTOS` в проектном
`.config` (в `defconfig` каждого шаблона из `templates/freertos/` он уже
включён). С FreeRTOS SDK предоставляет RAII-обёртки
в `sdk/rtos/include/rtos/rtos.hpp`.

## Конфигурация

- Heap: `heap_4`, 16 КБ по умолчанию — с v0.2.2 размер задаёт Kconfig-символ
  `FREERTOS_TOTAL_HEAP_SIZE` (по умолчанию `16384`).
- `configMINIMAL_STACK_SIZE`: 128 слов — с v0.2.2 Kconfig-символ
  `FREERTOS_MINIMAL_STACK_SIZE` (по умолчанию `128`).
- Частота тика, число приоритетов и параметры таймерной задачи — тоже
  Kconfig-символы (`FREERTOS_*`); дефолты воспроизводят исторические
  значения, см. [Конфигурацию](../configuration.ru.md).
- `SysTick` использует FreeRTOS — не определяйте `SysTick_Handler` отдельно.

## `rtos::Task`

Задача с управляемым lifetime'ом. Инстанс должен пережить запуск задачи —
типичный паттерн: function-static или namespace-scope объект после
`vTaskStartScheduler()`:

```cpp
import rtos; // приносит rtos::Task, rtos::Mutex, rtos::Semaphore

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

`rtos::Task::delay(ticks)` — современная обёртка над `vTaskDelay`.

## `rtos::Mutex` / `rtos::Semaphore`

```cpp
rtos::Mutex m;
{
  rtos::MutexGuard guard{m};  // scoped lock
  // критическая секция
}

rtos::Semaphore sem(0, 1);       // binary семафор, initial = 0
sem.give();                      // из ISR или задачи
sem.take();                      // блокирующий
sem.takeFromIsr(&hpw);           // из ISR
```

Не вызывайте FreeRTOS API **до** `vTaskStartScheduler()` — операции с
мьютексами / семафорами иначе зависают. Мы наступили на это при разработке
шаблона `i2c-scan`; шаблон был перенесён в `bare-metal/`
(`STM32_USE_FREERTOS` выключен в его `.config`), чтобы обойти проблему.

## Lifetime глобальных объектов

Общий паттерн в шаблонах:

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

`__initialize_hardware()` — weak-символ из SDK; приложение его перекрывает.
Он вызывается **до C++-конструкторов**, поэтому RCC-clocks включаются
своевременно, до инициализации глобальной периферии.

## ISR-handlers

Прямые ссылки на глобальные объекты, не указатели:

```cpp
extern "C" void USART2_IRQHandler() { g_uart2.irqHandler(); }
extern "C" void DMA1_Stream6_IRQHandler() { g_uart2.dmaTxIrqHandler(); }
```

Null-check семафоров внутри ISR защищают от spurious interrupt, выстрелившего
до завершения конструкции драйвера.
