# Логирование

Логирование с фильтрацией на этапе компиляции для bare-metal (с v0.1.14).
Поставляется вместе с библиотекой драйверов (`STM32_USE_DRIVERS`) и следует тому
же стилю zero-vtable / zero-heap, что и остальной SDK: backend вывода — это
captureless-указатель на функцию, никогда `std::function`, и без динамической
аллокации.

## Два уровня фильтрации

У логгера два независимых фильтра — именно это делает его дешёвым в релизе и
гибким на этапе отладки:

- **Compile-time (`STM32_LOG_LEVEL`).** Макросы `LOG_*` выше заданного порога
  раскрываются в `((void)0)`. Из бинарника убирается и вызов, *и его
  строка-формат*, поэтому заглушённые уровни стоят ноль flash и ноль тактов.
- **Runtime (`driver::log::setLevel`).** Из вызовов, переживших компиляцию,
  записи строго выше runtime-уровня отбрасываются без пересборки — удобно
  включать `Debug` по последовательной команде.

Запись выводится, только если её уровень `<=` **обоим** порогам.

## Уровни

| Уровень        | Значение | Префикс макроса |
|----------------|----------|-----------------|
| `None`         | 0        | (глушит всё)    |
| `Error`        | 1        | `LOG_ERROR`     |
| `Warn`         | 2        | `LOG_WARN`      |
| `Info`         | 3        | `LOG_INFO`      |
| `Debug`        | 4        | `LOG_DEBUG`     |
| `Trace`        | 5        | `LOG_TRACE`     |

## Использование

`import driver.log;`, затем подключите хедер с макросами **после** import
(макросы не экспортируются модулем C++20, поэтому живут в текстовом хедере — тот
же приём, что и `driver/try.hpp`):

```cpp
import driver.log;
import driver.log.uart;      // выбранный backend
import driver.uart;          // его шина, для UART-backend
import driver.stm32f4.uart;

#include "driver/log.hpp"    // макросы LOG_*, ПОСЛЕ import
```

Установите backend один раз при старте, задайте runtime-уровень и логируйте:

```cpp
Uart<> g_uart{*USART2, USART2_IRQn, uart<{...}>()};
driver::log::UartBackend g_logSink{g_uart};

int main() {
  g_logSink.install();
  driver::log::setLevel(driver::log::Level::Debug);

  LOG_INFO("boot", "system up");
  LOG_DEBUG_U32("adc", "raw=", reading);      // "D [adc] raw=1234"
  LOG_ERROR_HEX("reg", "CR1=", USART2->CR1);  // "E [reg] CR1=0x2000000c"
}
```

У каждого уровня три формы макроса:

- `LOG_INFO(tag, msg)` — просто сообщение.
- `LOG_INFO_U32(tag, msg, value)` — дописывает `uint32_t` в десятичном виде.
- `LOG_INFO_HEX(tag, msg, value)` — дописывает `uint32_t` как hex с префиксом
  `0x` в нижнем регистре.

Запись оформляется как `<L> [tag] msg\r\n`, где `<L>` — первая буква уровня
(`E`/`W`/`I`/`D`/`T`). Форматирование сделано вручную (без `printf`/`snprintf`),
поэтому работает под `nano.specs` и не тянет форматирование float.

## Выбор compile-time уровня

С v0.2.2 уровень — это Kconfig-choice в проектном `.config`
(`STM32_LOG_LEVEL_NONE` … `STM32_LOG_LEVEL_TRACE`, по умолчанию `INFO`) —
правится через `stmtool config`, см. [Конфигурацию](../configuration.ru.md).
Старая cache-переменная `-DSTM32_LOG_LEVEL=...` удалена.

Choice по-прежнему отображается в compile-define `STM32_LOG_LEVEL=<0..5>` на
цели драйверов; при отсутствии define хедер берёт `INFO`. Релизная сборка,
выбравшая `NONE` (или любой уровень ниже ваших вызовов
`LOG_DEBUG`/`LOG_TRACE`), вырезает эти операторы целиком.

## Backend'ы

Backend — это sink, thunk `void(*)(void* ctx, const char* data, size_t len)`,
устанавливаемый через `driver::log::setSink`. SDK поставляет два, оба с методом
`install()`:

- **`driver::log::ItmBackend`** (`import driver.log.itm`) — шлёт каждый байт в
  ITM stimulus port 0, читается отладчиком по SWO (OpenOCD `itm port 0 on`,
  ST-Link SWV). Без состояния. Если отладчик не включил трассировку,
  `ITM_SendChar` — no-op, поэтому устанавливать его безопасно безусловно.
- **`driver::log::UartBackend<UartDriver>`** (`import driver.log.uart`) — шлёт в
  любую реализацию `driver::IUart`, поэтому логи идут по той же последовательной
  линии, что и вывод приложения. Обобщён по шине в краткой constrained-форме;
  CTAD выводит параметр из конструктора (`UartBackend g_sink{g_uart}`).

Предполагаемый backend называется в `.config` через Kconfig-choice
`STM32_LOG_BACKEND_SEL_NONE` / `_ITM` / `_UART` (по умолчанию `NONE`) — там
же, где и уровень, см. [Конфигурацию](../configuration.ru.md).

Оба backend-модуля компилируются всегда (шаблон и inline-CMSIS ничего не стоят до
инстанцирования), поэтому choice не ограничивает доступность — он задаёт define
`STM32_LOG_BACKEND` (с символьными значениями `STM32_LOG_BACKEND_NONE` / `_ITM` /
`_UART`), позволяя коду приложения выбрать подходящий тип обобщённо:

```cpp
#if STM32_LOG_BACKEND == STM32_LOG_BACKEND_ITM
driver::log::ItmBackend g_logSink;
#else
driver::log::UartBackend g_logSink{g_uart};
#endif
```

## Свой backend

Подойдёт любая функция с сигнатурой sink. Устанавливается напрямую:

```cpp
void mySink(void* /*ctx*/, const char* data, size_t len) {
  for (size_t i = 0; i < len; ++i) semihostPut(data[i]);
}

driver::log::setSink(&mySink, nullptr);
```

`ctx` передаётся обратно как есть, поэтому backend с состоянием может протащить
через него `this` — ровно так `UartBackend` достаёт свой захваченный
`UartDriver&`.

## Host-тестирование

`driver.log` — CMSIS-free, поэтому компилируется и запускается на хосте.
`tests/host/` проверяет оформление записи, runtime-фильтр и десятичное/hex
форматирование против захватывающего sink — см. [Тестирование](testing.md).
