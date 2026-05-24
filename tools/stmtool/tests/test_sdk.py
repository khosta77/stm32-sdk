"""Tests for ``stmtool.sdk`` cache-management helpers."""

from __future__ import annotations

from pathlib import Path
from typing import Any
from unittest.mock import patch

import pytest

from stmtool import sdk as sdk_module


def _fake_completed(returncode: int = 0, stdout: str = "", stderr: str = "") -> Any:
    class _R:
        def __init__(self) -> None:
            self.returncode = returncode
            self.stdout = stdout
            self.stderr = stderr

    return _R()


def test_list_versions_parses_git_output(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(sdk_module, "_ensure_cache", lambda: tmp_path)
    with patch(
        "stmtool.sdk.subprocess.run",
        return_value=_fake_completed(0, "v0.1.3\nv0.1.2\nv0.1.1\n"),
    ):
        versions = sdk_module.list_versions()
    assert versions == ["v0.1.3", "v0.1.2", "v0.1.1"]


def test_list_versions_returns_empty_on_git_failure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(sdk_module, "_ensure_cache", lambda: tmp_path)
    with patch(
        "stmtool.sdk.subprocess.run",
        return_value=_fake_completed(returncode=128, stderr="fatal: not a git repo"),
    ):
        assert sdk_module.list_versions() == []


def test_current_version_returns_describe_output(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(sdk_module, "_ensure_cache", lambda: tmp_path)
    with patch(
        "stmtool.sdk.subprocess.run",
        return_value=_fake_completed(0, "v0.1.3-2-gabcdef\n"),
    ):
        assert sdk_module.current_version() == "v0.1.3-2-gabcdef"


def test_current_version_empty_on_failure(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(sdk_module, "_ensure_cache", lambda: tmp_path)
    with patch(
        "stmtool.sdk.subprocess.run",
        return_value=_fake_completed(returncode=128),
    ):
        assert sdk_module.current_version() == ""


def test_project_sdk_version_reads_toml(tmp_path: Path) -> None:
    (tmp_path / "stmproject.toml").write_text('[sdk]\nversion = "0.1.2"\n')
    assert sdk_module.project_sdk_version(tmp_path) == "0.1.2"


def test_project_sdk_version_missing_file_returns_none(tmp_path: Path) -> None:
    assert sdk_module.project_sdk_version(tmp_path) is None


def test_project_sdk_version_no_sdk_section(tmp_path: Path) -> None:
    (tmp_path / "stmproject.toml").write_text('[target]\nchip = "STM32F407VG"\n')
    assert sdk_module.project_sdk_version(tmp_path) is None


def test_project_sdk_version_handles_malformed_toml(tmp_path: Path) -> None:
    (tmp_path / "stmproject.toml").write_text("not = = valid")
    assert sdk_module.project_sdk_version(tmp_path) is None
