# Справочник `stmtool`

`stmtool` — Python-CLI для создания проектов, сборки прошивок и управления
кэшем SDK. С v0.2.1 это самостоятельный проект в отдельном репозитории
[`khosta77/stmtool`](https://github.com/khosta77/stmtool), устанавливается через
`pipx install git+https://github.com/khosta77/stmtool.git` (или `./install.sh`,
который пинит на последний релизный тег).

## Глобальные настройки

| Переменная | Описание |
|------------|----------|
| `STMSDK_PATH` | Явный путь к корню SDK. Перебивает поиск в кэше. |
| `STMTOOL_LANG` | Язык интерфейса: `en` (по умолчанию), `ru`. |

## Команды

### `stmtool project create`

```bash
stmtool project create <имя> --chip <чип> [--template <tpl>] [--with-claude]
```

Создать новый проект из шаблона.

| Флаг | По умолчанию | Описание |
|------|--------------|----------|
| `--chip` | обязательно | Чип STM32 в формате `STM32F4xxYZ` (например `STM32F407VG`) |
| `--template` | `blink` | Имя шаблона. См. `stmtool project templates`. |
| `--with-claude` | выкл. | Сгенерировать `CLAUDE.md` из `CLAUDE.md.template` шаблона |

Структура созданного проекта (зависит от шаблона):

```
<имя>/
  src/main.cpp
  CMakeLists.txt
  stmproject.toml
  .config            # копия defconfig шаблона
  CLAUDE.md          # только при --with-claude
  .gitignore
```

Поле `[sdk] version` в `stmproject.toml` по умолчанию `develop`. Отредактируйте,
чтобы зафиксировать конкретный тег.

### `stmtool project templates`

```bash
stmtool project templates
```

Показать все доступные шаблоны с категориями и описанием.

### `stmtool build`

```bash
stmtool build [--release] [--clean] [--chip <чип>] [--verbose]
```

Собрать проект в текущей директории. Любая сборка выполняется внутри Docker-образа
SDK — пути через хостовый тулчейн больше нет. Артефакты кладутся в `out/`.

| Флаг | По умолчанию | Описание |
|------|--------------|----------|
| `--release` | выкл. | Сборка с `-O2 -DNDEBUG` |
| `--clean` | выкл. | Очистить директорию `out/` перед сборкой |
| `--chip` | из `stmproject.toml` | Переопределить целевой чип |
| `--verbose` / `-v` | выкл. | Показывать полный вывод CMake/Ninja |

Docker-образ — `ghcr.io/khosta77/stm32-sdk-build:latest`, переопределяется
переменной окружения `STMTOOL_DOCKER_IMAGE` (в CI она указывает на локально
собранный образ). Образ содержит запиненный ARM-тулчейн и заранее подготовленный
FreeRTOS-Kernel, поэтому сборка не требует сети.

### `stmtool config`

```bash
stmtool config [--chip <чип>]
```

Открыть интерактивный Kconfig-**menuconfig** TUI (curses) для проекта в
текущей директории и записать результат в `.config` — единственный источник
правды о содержимом прошивки начиная с SDK v0.2.2 (см.
[Конфигурацию](configuration.ru.md)). Зависимости между опциями
контролируются прямо в UI; проект без `.config` стартует с дефолтов дерева,
что делает эту команду путём миграции для старых проектов. Чип берётся из
`stmproject.toml`, если не переопределён через `--chip`.

### `stmtool test`

```bash
stmtool test [--verbose]
```

Собрать и запустить host-юнит-тесты SDK (`tests/host`) внутри Docker-образа,
используя его хостовый `g++`. Они проверяют переносимый слой — `driver::Result<T>`,
`DRV_TRY` и логику драйверов / сенсоров на переиспользуемых мок-шинах — без железа.
Код возврата ненулевой при любом провале.

### `stmtool flash`

```bash
stmtool flash [--tool <tool>] [--verify] [--erase]
```

Прошить бинарник в подключённую плату.

| Флаг | По умолчанию | Описание |
|------|--------------|----------|
| `--tool` | `stlink` | Программатор (`stlink`, `jlink`, …, в зависимости от того, что установлено) |
| `--verify` | выкл. | Прочитать и сверить после записи. `st-flash` (бэкенд `stlink`) всегда сверяет каждую запись, поэтому там это подразумевается. |
| `--erase` | выкл. | Полное стирание перед записью |

### `stmtool sdk update`

```bash
stmtool sdk update [--version <тег>]
```

Обновить кэш SDK в `~/.stmtool/stm32-sdk/`.

- Без `--version` — использует `[sdk] version` из `stmproject.toml` текущей
  директории, если он есть, иначе `develop`.
- С `--version 0.1.2` — забирает тег `v0.1.2`.
- С `--version develop` — pull последних изменений с `origin/develop`.

### `stmtool sdk list-versions`

```bash
stmtool sdk list-versions
```

Печатает все доступные теги SDK (сортировка по убыванию). Текущая
checked-out версия помечается `*` в конце строки.

### `stmtool sdk path`

```bash
stmtool sdk path
```

Печатает резолвнутый путь к корню SDK — удобно для отладки и скриптов.

### `stmtool doctor`

```bash
stmtool doctor
```

Проверяет наличие в `PATH`: Docker, `arm-none-eabi-gcc`, `cmake`, `st-flash`,
а также присутствует ли SDK-образ локально. Локальный `arm-none-eabi-gcc` ниже
GCC 14 отмечается как ошибка (сканирование модулей C++20 требует GCC >= 14).

### `stmtool completion`

```bash
stmtool completion <shell>
```

Печатает скрипт shell-completion для `bash`, `zsh` или `fish`. `install.sh`
устанавливает его автоматически.

### `stmtool show-version`

```bash
stmtool show-version
```

Печатает версию `stmtool`. Версия — `0.N`: minor авто-инкрементит CI на каждый
merge в `master` репозитория `stmtool` (`poetry-dynamic-versioning` читает тег
`v0.N`). Независима от релизного тега SDK.

## Порядок поиска SDK

Когда команде нужен корень SDK, `stmtool` проверяет в порядке:

1. Переменную окружения `STMSDK_PATH`.
2. Родительские директории исполняемого `stmtool`, содержащие `sdk/` и
   `templates/` (для checkout-сборок).
3. Кэш `~/.stmtool/stm32-sdk/` — создаётся первой командой через
   `git clone` репозитория.

Если кэш существует, `stmtool` вызывает `_checkout_version()` для нужной версии.
