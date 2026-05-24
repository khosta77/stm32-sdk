"""Tests for ``stmtool.config.load_config``."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

from stmtool.config import load_config

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib


def test_load_config_parses_target_chip(tmp_path: Path) -> None:
    cfg = tmp_path / "stmproject.toml"
    cfg.write_text('[target]\nchip = "STM32F407VG"\n')
    data = load_config(cfg)
    assert data["target"]["chip"] == "STM32F407VG"


def test_load_config_parses_sdk_version(tmp_path: Path) -> None:
    cfg = tmp_path / "stmproject.toml"
    cfg.write_text('[sdk]\nversion = "0.1.3"\n')
    data = load_config(cfg)
    assert data["sdk"]["version"] == "0.1.3"


def test_load_config_missing_file_raises(tmp_path: Path) -> None:
    with pytest.raises(FileNotFoundError):
        load_config(tmp_path / "does-not-exist.toml")


def test_load_config_invalid_toml_raises(tmp_path: Path) -> None:
    cfg = tmp_path / "stmproject.toml"
    cfg.write_text("this is = not = valid toml\n")
    with pytest.raises(tomllib.TOMLDecodeError):
        load_config(cfg)
