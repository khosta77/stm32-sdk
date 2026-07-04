# Testing — юнит-тесты на устройстве

С версии v0.1.11 SDK поставляет минимальный тест-фреймворк в стиле GTest —
единственный заголовок `testing/unit_test.hpp` в `sdk/testing/include/`. Он даёт
макросы-проверки `ASSERT_*` / `EXPECT_*` и крошечный раннер, который печатает
pass/fail в любой предоставленный вами символьный сток.

Он рассчитан на bare-metal: **без исключений, без RTTI, без heap, без
динамической линковки** и **без зависимости от `<cstdio>` / `snprintf`** (целые
форматируются вручную). Поскольку сток вывода инжектируется, один и тот же код
тестов компилируется и гоняется и на хосте (сток → `stdout`), и на устройстве
(сток → UART / SWO). Дополняет мок-шины и host-тесты CI (issues #34, #35): те
дают host-раннер, а это — общий для обоих словарь проверок.

Включается флагом `STM32_USE_TESTING`, линкуется header-only таргет
`stm32_testing`. См. [флаги сборки](../build-flags.md).

## Написание тестов

Тест — это обычная функция, объявленная через `TEST(name)`; макросы `ASSERT_*` /
`EXPECT_*` работают с неявным контекстом теста `_t`.

```cpp
#include "testing/unit_test.hpp"
import driver.types;
using driver::Result;
using driver::Status;

TEST(result_ok_carries_value) {
  Result<int> r{42};
  ASSERT_TRUE(r.ok());       // при провале останавливает этот тест
  EXPECT_EQ(r.value(), 42);  // фиксирует провал, продолжает
  EXPECT_EQ(r.valueOr(0), 42);
}

TEST(result_error_reports_status) {
  Result<int> r{Status::Timeout};
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status(), Status::Timeout);
  EXPECT_EQ(r.valueOr(7), 7);
}
```

- `EXPECT_*` фиксирует провал и продолжает тест.
- `ASSERT_*` фиксирует провал и немедленно делает `return` из теста — остаток
  тела теста не выполняется. На платформе без исключений это ровно то, как ведут
  себя фатальные проверки GTest; здесь тест *является* функцией, поэтому обычного
  `return` достаточно.

Доступные проверки: `ASSERT_TRUE` / `ASSERT_FALSE` / `ASSERT_EQ` / `ASSERT_NE` и
их аналоги `EXPECT_`. Они типонезависимы — работают с целыми, указателями,
`bool` и enum'ами вроде `driver::Status`.

## Запуск тестов

**Авто-регистрации нет** — вызывающий задаёт явный список запуска. Это делает
порядок детерминированным и избегает статических конструкторов до `main()` на
bare-metal.

```cpp
void uartWrite(const char *s);  // ваш сток: шлёт байты в USART

int main() {
  testing::TestRunner runner{uartWrite};

  RUN_TEST(runner, result_ok_carries_value);
  RUN_TEST(runner, result_error_reports_status);

  const bool ok = runner.summary();  // «N passed, M failed»
  // ok == true, когда все тесты прошли
  while (true) {
  }
}
```

- `testing::TestRunner runner{writer}` — `writer` имеет тип
  `testing::Writer` = `void (*)(const char *)`.
- `RUN_TEST(runner, name)` гоняет один тест и учитывает результат, печатая
  `[PASS] name` или `[FAIL] name` (со строкой `  fail: <выражение>` на каждую
  проваленную проверку).
- `runner.summary()` печатает `N passed, M failed` и возвращает `true`, когда
  `failed() == 0`. `passed()` / `failed()` дают счётчики.

## Хост и устройство из одного исходника

Сток — единственная платформо-зависимая часть. На устройстве это UART-writer;
на хосте направьте его в `stdout`, и тот же файл тестов собирается штатным
компилятором:

```cpp
#include <cstdio>
#include "testing/unit_test.hpp"

static void stdoutWriter(const char *s) {
  std::fputs(s, stdout);
}

int main() {
  testing::TestRunner runner{stdoutWriter};
  RUN_TEST(runner, result_ok_carries_value);
  return runner.summary() ? 0 : 1;  // ненулевой код возврата при любом провале
}
```

Шаблон `bare-metal/unit-test-demo` — рабочий пример для устройства; его тесты
проверяют `driver::Result<T>` — чистую логику, поэтому они осмысленны на любой
из платформ.

## Мок-шины (v0.1.13)

Поскольку интерфейсы шин — это **концепты** C++20, мок — это обычный struct,
удовлетворяющий концепту. SDK поставляет переиспользуемые программируемые моки в
модуле `testing.mock` (`sdk/testing/mock/mock_bus.cppm`):

| Мок | Моделирует | Программируется через |
|-----|------------|------------------------|
| `testing::MockI2c` | `driver::II2c` | `loadResponse(...)`, `written()`, `nextStatus` |
| `testing::MockSpi` | `driver::ISpi` | `loadResponse(...)`, `written()`, `nextStatus` |
| `testing::MockUart` | `driver::IUart` | `loadRx(...)`, `transmitted()` |
| `testing::MockGpioPin` | `driver::IGpioPin` | счётчики `set/reset/toggle`, уровень |
| `testing::MockFlash` | `driver::IFlash` | массив в памяти, `eraseSector` |

`loadResponse` / `loadRx` задают байты, которые вернёт чтение; `written()` /
`transmitted()` показывают, что отправил тестируемый код, — так проверяется
поведение, а не только код возврата. `nextStatus` инжектирует ошибку шины. Моки
без кучи и исключений, поэтому компилируются и на устройстве.

```cpp
import sensor.mpu6050;
import testing.mock;

TEST(mpu6050_scales_accel) {
  testing::MockI2c bus;
  const uint8_t frame[14] = {0x40, 0x00};  // ACCEL_XOUT hi/lo -> 1 g
  bus.loadResponse({frame, 14});

  sensor::Mpu6050 mpu{bus, kCfg};
  sensor::ImuData data;
  ASSERT_EQ(mpu.read(data), Status::Ok);
  EXPECT_EQ(bus.lastReg, 0x3B);  // обратился к ACCEL_XOUT_H
  EXPECT_TRUE(data.accel.x > 9.8f && data.accel.x < 9.82f);
}
```

Модуль не линкуется в INTERFACE-таргет `stm32_testing` (он импортирует
driver-концепты); потребитель подключает его, добавив `mock_bus.cppm` в свой
набор `CXX_MODULES` рядом со `stm32_drivers`.

## Host-тесты и `stmtool test` (v0.1.13)

`tests/host/` — отдельное CMake-дерево, собираемое хостовым `g++` из образа: оно
компилирует CMSIS-free переносимые модули плюс `testing.mock` в исполняемые файлы
и запускает их под `ctest`. Прогнать весь набор:

```bash
stmtool test
```

Команда собирает и запускает тесты внутри Docker-образа SDK (в `out/host`) и
возвращает ненулевой код при любом провале; CI выполняет ту же команду в job
`host-tests`. Чтобы добавить случай, положите `test_*.cpp` в `tests/host/` и
зарегистрируйте его в `tests/host/CMakeLists.txt` через `add_host_test(...)`.

Два правила, чтобы модули C++20 не ломались в TU, который одновременно импортирует
модули и включает текстовые заголовки:

- **Включайте STL / текстовые заголовки до строк `import`.** Модуль включает,
  например, `<cstddef>` в global module fragment; текстовое включение *после*
  импорта такого модуля вызывает `redefinition of std::__byte_operand`.
- **Не пишите `#include <span>`; инициализируйте span через фигурные скобки**
  (`bus.loadResponse({p, n})`) и используйте `auto` для возвращаемых span —
  смешение `#include <span>` с `import` модуля, включающего `<span>`, отвергается
  GCC 15.
