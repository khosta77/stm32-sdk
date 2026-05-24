"""Minimal English/Russian translation table for stmtool CLI strings."""

import os

_LANG = os.environ.get("STMTOOL_LANG", "en").lower()[:2]

_STRINGS: dict[str, dict[str, str]] = {
    "app_help": {
        "en": "STM32-SDK project tool",
        "ru": "Инструмент для проектов STM32-SDK",
    },
    "project_help": {
        "en": "Project management",
        "ru": "Управление проектами",
    },
    "project_create_help": {
        "en": "Create a new STM32 project from template",
        "ru": "Создать новый проект STM32 из шаблона",
    },
    "build_help": {
        "en": "Build the project (Docker by default)",
        "ru": "Собрать проект (Docker по умолчанию)",
    },
    "build_release": {
        "en": "Release build",
        "ru": "Релизная сборка",
    },
    "build_native": {
        "en": "Build locally without Docker",
        "ru": "Собрать локально без Docker",
    },
    "build_verbose": {
        "en": "Verbose CMake output",
        "ru": "Подробный вывод CMake",
    },
    "build_chip": {
        "en": "Override chip from stmproject.toml",
        "ru": "Переопределить чип из stmproject.toml",
    },
    "flash_help": {
        "en": "Flash firmware to target board",
        "ru": "Прошить прошивку на плату",
    },
    "flash_tool": {
        "en": "Flash tool: stlink, openocd, pyocd, jlink",
        "ru": "Инструмент прошивки: stlink, openocd, pyocd, jlink",
    },
    "flash_verify": {
        "en": "Verify after flash",
        "ru": "Проверить после прошивки",
    },
    "flash_erase": {
        "en": "Erase flash before writing",
        "ru": "Стереть flash перед записью",
    },
    "doctor_help": {
        "en": "Check development environment",
        "ru": "Проверить окружение разработки",
    },
    "version_help": {
        "en": "Show stmtool version",
        "ru": "Показать версию stmtool",
    },
    "project_name_help": {
        "en": "Project name",
        "ru": "Имя проекта",
    },
    "chip_help": {
        "en": "Target STM32 chip (e.g. STM32F407VG)",
        "ru": "Целевой чип STM32 (напр. STM32F407VG)",
    },
    "template_help": {
        "en": "Template name",
        "ru": "Имя шаблона",
    },
    "not_implemented": {
        "en": "Not yet implemented",
        "ru": "Ещё не реализовано",
    },
    "no_chip": {
        "en": "Error: no chip specified (use --chip or set target.chip in stmproject.toml)",
        "ru": (
            "Ошибка: чип не указан (используйте --chip или задайте target.chip в stmproject.toml)"
        ),
    },
    "building": {
        "en": "Building {chip} ({build_type}){mode}...",
        "ru": "Сборка {chip} ({build_type}){mode}...",
    },
    "mode_local": {
        "en": " locally",
        "ru": " локально",
    },
    "mode_docker": {
        "en": " in Docker",
        "ru": " в Docker",
    },
    "flashing": {
        "en": "Flashing {name} via st-link...",
        "ru": "Прошивка {name} через st-link...",
    },
    "no_bin": {
        "en": "Error: no .bin file found in build/. Run 'stmtool build' first.",
        "ru": "Ошибка: файл .bin не найден в build/. Сначала запустите 'stmtool build'.",
    },
    "project_created": {
        "en": "Project '{name}' created at {path}",
        "ru": "Проект '{name}' создан в {path}",
    },
    "project_exists": {
        "en": "Error: directory '{name}' already exists",
        "ru": "Ошибка: директория '{name}' уже существует",
    },
    "sdk_not_found": {
        "en": "Error: SDK not found. Check network connection or set STMSDK_PATH.",
        "ru": "Ошибка: SDK не найден. Проверьте сеть или установите STMSDK_PATH.",
    },
    "sdk_cloning": {
        "en": "Downloading SDK to ~/.stmtool/stm32-sdk...",
        "ru": "Загрузка SDK в ~/.stmtool/stm32-sdk...",
    },
    "sdk_downloading": {
        "en": "Downloading SDK...",
        "ru": "Загрузка SDK...",
    },
    "completion_help": {
        "en": "Show shell completion script (bash/zsh/fish)",
        "ru": "Показать скрипт автодополнения (bash/zsh/fish)",
    },
    "completion_shell_help": {
        "en": "Shell type",
        "ru": "Тип оболочки",
    },
    "completion_hint": {
        "en": "Add this to your shell config to enable autocompletion.",
        "ru": "Добавьте это в конфигурацию вашей оболочки для автодополнения.",
    },
    "template_not_found": {
        "en": "Error: template '{name}' not found. Available: {available}",
        "ru": "Ошибка: шаблон '{name}' не найден. Доступные: {available}",
    },
    "invalid_chip": {
        "en": "Error: invalid chip format '{chip}'",
        "ru": "Ошибка: неверный формат чипа '{chip}'",
    },
    "initializing_git": {
        "en": "Creating project...",
        "ru": "Создание проекта...",
    },
    "build_clean": {
        "en": "Clean build directory before building",
        "ru": "Очистить директорию сборки перед сборкой",
    },
    "cleaning": {
        "en": "Cleaning build directory...",
        "ru": "Очистка директории сборки...",
    },
    "create_claude_help": {
        "en": "Also generate a CLAUDE.md tailored to the template",
        "ru": "Сгенерировать CLAUDE.md, адаптированный под шаблон",
    },
    "create_claude_added": {
        "en": "CLAUDE.md generated from the template",
        "ru": "CLAUDE.md создан из шаблона",
    },
    "create_claude_missing": {
        "en": "Warning: template '{template}' has no CLAUDE.md.template — skipping",
        "ru": "Предупреждение: у шаблона '{template}' нет CLAUDE.md.template — пропускаем",
    },
    "sdk_help": {
        "en": "Manage the cached SDK",
        "ru": "Управление кэшем SDK",
    },
    "sdk_update_help": {
        "en": "Update the cached SDK to a release tag or develop",
        "ru": "Обновить кэш SDK до релизного тега или develop",
    },
    "sdk_update_version_help": {
        "en": (
            "SDK version (release tag like 0.1.2 or 'develop'); "
            "defaults to [sdk] version in stmproject.toml"
        ),
        "ru": (
            "Версия SDK (тег вида 0.1.2 или 'develop'); "
            "по умолчанию — [sdk] version из stmproject.toml"
        ),
    },
    "sdk_list_versions_help": {
        "en": "List available SDK versions (git tags)",
        "ru": "Показать доступные версии SDK (git-теги)",
    },
    "sdk_path_help": {
        "en": "Print the resolved SDK root path",
        "ru": "Показать резолвнутый путь к корню SDK",
    },
    "sdk_updating": {
        "en": "Updating SDK to '{version}'...",
        "ru": "Обновление SDK до '{version}'...",
    },
    "sdk_updated": {
        "en": "SDK updated to '{version}' at {path}",
        "ru": "SDK обновлён до '{version}' в {path}",
    },
    "sdk_no_tags_found": {
        "en": "No release tags found in the SDK cache",
        "ru": "В кэше SDK не найдено релизных тегов",
    },
}


def t(key: str, **kwargs: str) -> str:
    """Translate ``key`` into the active language and substitute ``kwargs``."""
    entry = _STRINGS.get(key, {})
    text = entry.get(_LANG, entry.get("en", key))
    if kwargs:
        text = text.format(**kwargs)
    return text
