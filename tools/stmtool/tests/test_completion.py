"""Tests for stmtool autocomplete callbacks."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

from stmtool.completions import complete_chip, complete_flash_tool, complete_template


def test_complete_chip_prefix_match() -> None:
    suggestions = complete_chip("STM32F407")
    assert "STM32F407VE" in suggestions
    assert "STM32F407VG" in suggestions
    assert all(s.startswith("STM32F407") for s in suggestions)


def test_complete_chip_case_insensitive() -> None:
    suggestions = complete_chip("stm32f411")
    assert "STM32F411CE" in suggestions
    assert "STM32F411RE" in suggestions


def test_complete_chip_no_match_returns_empty() -> None:
    assert complete_chip("STM99") == []


def test_complete_flash_tool_prefix_match() -> None:
    assert complete_flash_tool("st") == ["stlink"]
    assert "openocd" in complete_flash_tool("o")


def test_complete_flash_tool_empty_returns_all() -> None:
    assert set(complete_flash_tool("")) == {"stlink", "openocd", "pyocd", "jlink"}


def test_complete_template_falls_back_on_resolve_error() -> None:
    with patch(
        "stmtool.completions.resolve_sdk_root",
        side_effect=RuntimeError("SDK not found"),
    ):
        assert complete_template("bl") == ["blink"]


def test_complete_template_scans_template_dir(tmp_template_dir: Path) -> None:
    sdk_root = tmp_template_dir.parents[2]
    with patch("stmtool.completions.resolve_sdk_root", return_value=sdk_root):
        result = complete_template("bl")
    assert result == ["blink"]


def test_complete_template_filters_by_prefix(tmp_template_dir: Path) -> None:
    sdk_root = tmp_template_dir.parents[2]
    with patch("stmtool.completions.resolve_sdk_root", return_value=sdk_root):
        assert complete_template("xyz") == []
