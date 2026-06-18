# Быстрый старт

## Что нужно установить

| Инструмент | Минимальная версия | Назначение |
|------------|---------------------|-----------|
| `arm-none-eabi-gcc` | 14 | Кросс-компилятор для Cortex-M (14+ нужен для сканирования модулей C++20) |
| `cmake` | 3.28 | Поддержка модулей C++20 |
| `python` | 3.10 | Запускает `stmtool` |
| `pipx` | любая | Изолированная установка `stmtool` (рекомендуется) |
| `st-flash` | любая | Опционально, прошивка через ST-Link |

Проверить окружение:

```bash
stmtool doctor
```

## Установка `stmtool`

Предпочтительный способ — скрипт `install.sh`. Он удалит предыдущую установку,
очистит кэш SDK, поставит последний релизный тег и настроит shell completion:

```bash
./install.sh
```

Альтернативно, установка напрямую из клона репозитория:

```bash
pip install ./tools/stmtool
```

## Первый проект

```bash
stmtool project create my-blink --chip STM32F407VG
cd my-blink
```

Структура сгенерированного проекта:

```
my-blink/
  src/main.cpp        # код приложения
  CMakeLists.txt      # конфигурация сборки
  stmproject.toml     # выбор чипа и привязка к версии SDK
  .gitignore
```

По умолчанию используется `--template blink` (bare-metal). Список всех шаблонов:

```bash
stmtool project templates
```

Чтобы начать с FreeRTOS-примера (например, MPU6050 + UART DMA):

```bash
stmtool project create imu --chip STM32F407VG --template mpu6050-uart
```

Чтобы также сгенерировать `CLAUDE.md` под этот конкретный шаблон (распиновка,
ожидаемый serial-выход, шаги верификации):

```bash
stmtool project create imu --chip STM32F407VG \
  --template mpu6050-uart --with-claude
```

## Сборка

Локально (использует `arm-none-eabi-gcc` и `cmake` хоста):

```bash
stmtool build --native
```

Или через Docker (тулчейн на хосте не нужен):

```bash
stmtool build
```

## Прошивка

```bash
stmtool flash
```

По умолчанию используется `st-flash`. Если у вас другой программатор —
переопределите через `--tool`.

## Обновление SDK позже

Каждый проект фиксирует версию SDK в `stmproject.toml`:

```toml
[sdk]
version = "develop"   # или тег, например "0.1.2"
```

Чтобы обновить кэш SDK:

```bash
stmtool sdk update                       # использует [sdk] version
stmtool sdk update --version 0.1.2       # явный тег
stmtool sdk list-versions                # доступные теги
```

## Что дальше

- Прочитать [справочник по stmtool](stmtool.md) — все команды.
- Посмотреть API [драйверов](modules/drivers.md) и [сенсоров](modules/sensors.md).
- Перед апгрейдом SDK — [заметки по апгрейду](migration.md).
