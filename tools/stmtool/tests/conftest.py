"""Shared pytest fixtures for the stmtool test suite."""

from __future__ import annotations

import os
from collections.abc import Iterator
from pathlib import Path

import pytest


@pytest.fixture
def tmp_sdk_root(tmp_path: Path) -> Path:
    """Build a minimal directory layout that ``_is_sdk_root`` accepts."""
    sdk = tmp_path / "sdk-fake"
    (sdk / "sdk").mkdir(parents=True)
    (sdk / "templates").mkdir()
    return sdk


@pytest.fixture
def tmp_template_dir(tmp_sdk_root: Path) -> Path:
    """Create a single ``blink``-style template under ``tmp_sdk_root``."""
    tpl = tmp_sdk_root / "templates" / "bare-metal" / "blink"
    tpl.mkdir(parents=True)
    (tpl / "template.toml").write_text(
        '[template]\nname = "blink"\ndescription = "LED blink demo"\ncategory = "bare-metal"\n'
    )
    (tpl / "CMakeLists.txt.template").write_text(
        "project(@PROJECT_NAME@)\nset(STM32_CHIP @STM32_CHIP@)\nset(SDK_VERSION @SDK_VERSION@)\n"
    )
    (tpl / "main.cpp").write_text("int main() { return 0; }\n")
    return tpl


@pytest.fixture
def project_workdir(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Iterator[Path]:
    """Cd into a clean temporary directory for the duration of the test."""
    monkeypatch.chdir(tmp_path)
    yield tmp_path


@pytest.fixture
def stmsdk_env(tmp_sdk_root: Path, monkeypatch: pytest.MonkeyPatch) -> Iterator[Path]:
    """Point ``STMSDK_PATH`` at ``tmp_sdk_root`` for the test."""
    monkeypatch.setenv("STMSDK_PATH", str(tmp_sdk_root))
    yield tmp_sdk_root
    monkeypatch.delenv("STMSDK_PATH", raising=False)


@pytest.fixture
def clean_stmtool_lang(monkeypatch: pytest.MonkeyPatch) -> None:
    """Force English locale so message assertions are stable."""
    monkeypatch.setenv("STMTOOL_LANG", "en")
    os.environ["STMTOOL_LANG"] = "en"
