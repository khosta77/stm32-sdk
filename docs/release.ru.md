# Релизы

## Политика версионирования

Теги используют SemVer (`vMAJOR.MINOR.PATCH`). Проект находится в pre-1.0:
и patch-, и minor-релизы могут включать небольшие изменения, требующие правок
в пользовательском коде. Где именно проходит граница «patch vs minor» — решает
maintainer, пока API не стабилизируется (цель — v1.0).

Практические рекомендации для подключающих SDK проектов:

- Фиксируйте конкретный тег в `stmproject.toml` (`[sdk] version = "0.1.2"`),
  а не `develop`.
- Перед апгрейдом — прочитайте [заметки по апгрейду](migration.md).
- Используйте `stmtool sdk update --version <тег>` для обновления кэша.

## Инструменты

Номер версии выводится из git-тегов через `poetry-dynamic-versioning`
(переход с `setuptools-scm` в v0.1.4). Нигде нет ручной константы —
именно тег создаёт релиз для пользователей `stmtool`.

`tools/stmtool/pyproject.toml` конфигурирует:

```toml
[build-system]
requires = ["poetry-core>=1.9", "poetry-dynamic-versioning>=1.4"]
build-backend = "poetry_dynamic_versioning.backend"

[tool.poetry-dynamic-versioning]
enable = true
vcs = "git"
style = "pep440"
```

Плагин поднимается от `tools/stmtool/pyproject.toml` до корневого
`.git` и берёт последний `vMAJOR.MINOR.PATCH` тег — тот же источник
истины, что использует и CMake-сторона SDK.

## Процедура релиза

1. `develop` должен быть зелёным на CI (matrix F407VG/F401CE/F411CE).
2. Локально проверить доки: `mkdocs serve`.
3. Повесить тег на merge-коммит в `develop`:
   ```bash
   git tag -a v0.1.3 -m "Release v0.1.3" <коммит>
   git push origin v0.1.3
   ```
4. Создать GitHub Release с заметками из секции «История релизов» этой страницы.
5. Workflow `docs.yml` пересоберёт сайт автоматически — проверить, что
   <https://khosta77.github.io/stm32-sdk/> подтянул свежий контент.

## История релизов

### v0.1.4 (в подготовке)

Фокус: качество и инфраструктура, новых SDK-фич нет кроме
[issue #9](https://github.com/khosta77/stm32-sdk/issues/9).

Главные изменения:

- `driver::NullGpioPin` — пустая реализация `IGpioPin` для плат, где
  CS-линия SPI впаяна в GND (issue #9). Drop-in для сенсоров,
  принимающих `IGpioPin&` (например, `W25q32`).
- `stmtool` мигрирован с `setuptools` на Poetry + `poethepoet`.
  Новая команда `poetry run poe ci` запускает ruff / flake8 / pylint
  / black / isort / mypy (strict) / bandit / pytest за один заход.
  `poetry run poe fix` применяет все авто-фиксеры. Рантайм `__version__`
  теперь берётся через `importlib.metadata`; генерируемый `_version.py`
  удалён.
- У `stmtool` появился baseline тест-сьют (~80% покрытия, 50 тестов)
  и новый workflow `.github/workflows/stmtool.yml`, запускающий
  `poe ci` на каждом PR, затрагивающем `tools/stmtool/**`.
- `.github/workflows/build.yml` переписан: chip-matrix теперь
  `[STM32F407VG]`, и каждый PR собирает **все 7 шаблонов** параллельно
  (`fail-fast: false`).
- `-Wall -Wextra -Wpedantic -Wshadow -Werror` постоянно включены на
  `stm32_core` и пропагируются на драйверы, RTOS, сенсоры, пользовательский
  код. Политика — на странице [Compiler and warning flags](build-flags.md).
- Project-level `.claude/commands/{ci,fix,build-all-templates,test-template,release-check}.md`
  для контрибьюторов, использующих Claude Code.
- Новая секция `CLAUDE.md` «Quality enforcement»: линт-правила и `-W*`
  флаги нельзя отключать без явного согласования с пользователем.

Известные ограничения / не проверяется в CI:

- **STM32F401CE и STM32F411CE** формально поддерживаются кодом SDK, но
  больше не покрываются CI (у maintainer'а нет физического железа).
  Используйте на свой страх и риск; если соберёте под них и поймаете
  warning, ломающий `-Werror` — заведите issue.
- `bare-metal/blink/main.cpp` обновлён: `g_ticks = g_ticks + 1` вместо
  `++g_ticks` (C++20 deprecation `operator++` на volatile). Downstream-
  проектам, скопировавшим оригинальный код, нужна такая же однострочная
  правка.

Отложено в v0.1.5+:

- Все P1-команды `stmtool` из
  [issue #2](https://github.com/khosta77/stm32-sdk/issues/2):
  `monitor`, `size`, `config`, `project info`, `device list`, `chips`,
  `boards`.
- Новые bare-metal шаблоны: `uart-echo`, `spi-sensor`, `adc-dma`.
- Board definitions (`.toml` файлы) и Python chipdb.
- Флаг `-Wconversion` (много шума на CMSIS-коде; требует отдельной
  чистки).

### v0.1.3 (в подготовке)

Главные изменения:

- Новый сайт документации (MkDocs Material), английский и русский.
- Подкоманды `stmtool sdk update`, `sdk list-versions`, `sdk path`.
- Опциональная генерация `CLAUDE.md` per-template через `--with-claude`.
- Документированы все новые сенсоры (MPU6050, SSD1306, W25Q32) и DMA-обёртка.
- Исправлен I2C multi-byte read по RM0090 §27.3.3, корректен на 400 кГц.
- `GpioConfig` переписан как aggregate с валидатором `consteval gpio({...})`.

См. [заметки по апгрейду](migration.md) о необходимых правках в пользовательском коде.

### v0.1.2

- Усиление `stmtool` install (`install.sh` чистит кэш при переустановке).

### v0.1.1

- FreeRTOS-шаблоны (`freertos/blink`, `freertos/mpu6050-uart`) зарегистрированы
  для discovery в `stmtool`.

### v0.1.0

- Первый релиз. Версионирование через `setuptools-scm` для `stmtool` и SDK
  в целом. CI перенесён с `main` на `develop`.

## GitHub Pages

Сайт документации публикуется из ветки `gh-pages` (создаётся workflow
`docs.yml`). Адрес — <https://khosta77.github.io/stm32-sdk/>. После первого
деплоя — включить Pages в Settings → Pages → Source = `gh-pages` / `(root)`.
