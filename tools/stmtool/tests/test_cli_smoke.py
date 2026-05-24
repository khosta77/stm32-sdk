"""End-to-end smoke tests for the Typer-based stmtool CLI."""

from __future__ import annotations

from pathlib import Path
from typing import Any
from unittest.mock import patch

import pytest
from typer.testing import CliRunner

from stmtool.cli import app

runner = CliRunner()


def test_show_version_prints_version_string() -> None:
    result = runner.invoke(app, ["show-version"])
    assert result.exit_code == 0
    assert "stmtool" in result.stdout


def test_project_templates_lists_templates(tmp_template_dir: Path, stmsdk_env: Path) -> None:
    result = runner.invoke(app, ["project", "templates"])
    assert result.exit_code == 0
    assert "blink" in result.stdout


def test_project_templates_empty_exits_nonzero(stmsdk_env: Path) -> None:
    result = runner.invoke(app, ["project", "templates"])
    assert result.exit_code != 0


def test_doctor_handles_missing_tools() -> None:
    with patch("stmtool.cli.subprocess.run", side_effect=FileNotFoundError):
        result = runner.invoke(app, ["doctor"])
    assert result.exit_code == 0
    assert "NOT FOUND" in result.stdout


def test_doctor_reports_versions() -> None:
    class _Result:
        returncode = 0
        stdout = "cmake version 3.28.0\n"
        stderr = ""

    with patch("stmtool.cli.subprocess.run", return_value=_Result()):
        result = runner.invoke(app, ["doctor"])
    assert result.exit_code == 0
    assert "OK" in result.stdout


def test_flash_no_bin_files(project_workdir: Path) -> None:
    (project_workdir / "build").mkdir()
    result = runner.invoke(app, ["flash"])
    assert result.exit_code == 1


def test_sdk_list_versions_empty_exits_nonzero() -> None:
    with patch("stmtool.cli.sdk_module.list_versions", return_value=[]):
        result = runner.invoke(app, ["sdk", "list-versions"])
    assert result.exit_code == 1


def test_sdk_list_versions_prints_tags() -> None:
    with (
        patch(
            "stmtool.cli.sdk_module.list_versions",
            return_value=["v0.1.3", "v0.1.2"],
        ),
        patch(
            "stmtool.cli.sdk_module.current_version",
            return_value="v0.1.3",
        ),
    ):
        result = runner.invoke(app, ["sdk", "list-versions"])
    assert result.exit_code == 0
    assert "v0.1.3" in result.stdout
    assert "v0.1.2" in result.stdout


def test_sdk_update_calls_module(project_workdir: Path) -> None:
    with (
        patch(
            "stmtool.cli.sdk_module.update_cache",
            return_value=project_workdir / "fake-sdk",
        ),
        patch(
            "stmtool.cli.sdk_module.project_sdk_version",
            return_value=None,
        ),
    ):
        result = runner.invoke(app, ["sdk", "update", "--version", "0.1.3"])
    assert result.exit_code == 0


def test_sdk_update_handles_runtime_error() -> None:
    with (
        patch(
            "stmtool.cli.sdk_module.update_cache",
            side_effect=RuntimeError("network"),
        ),
        patch(
            "stmtool.cli.sdk_module.project_sdk_version",
            return_value=None,
        ),
    ):
        result = runner.invoke(app, ["sdk", "update", "--version", "0.1.3"])
    assert result.exit_code == 1


def test_sdk_path_cmd_prints_resolved_path(tmp_path: Path) -> None:
    with patch("stmtool.cli.sdk_module.resolve_path", return_value=tmp_path):
        result = runner.invoke(app, ["sdk", "path"])
    assert result.exit_code == 0
    assert str(tmp_path) in result.stdout


def test_sdk_path_cmd_handles_error() -> None:
    with patch(
        "stmtool.cli.sdk_module.resolve_path",
        side_effect=RuntimeError("missing"),
    ):
        result = runner.invoke(app, ["sdk", "path"])
    assert result.exit_code == 1


def test_build_without_chip_exits_nonzero(
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        "stmtool.cli.resolve_sdk_root",
        lambda **_: project_workdir / "fake-sdk",
    )
    (project_workdir / "fake-sdk").mkdir()
    result = runner.invoke(app, ["build", "--native"])
    assert result.exit_code != 0


def test_build_native_invokes_cmake(
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        "stmtool.cli.resolve_sdk_root",
        lambda **_: project_workdir / "fake-sdk",
    )
    (project_workdir / "fake-sdk").mkdir()
    captured: list[list[str]] = []

    def fake_run(cmd: Any, **_kwargs: Any) -> Any:
        captured.append(list(cmd))

        class _R:
            returncode = 0

        return _R()

    monkeypatch.setattr("stmtool.cli.subprocess.run", fake_run)
    result = runner.invoke(app, ["build", "--native", "--chip", "STM32F407VG"])
    assert result.exit_code == 0
    assert any("cmake" in c[0] for c in captured)


def test_flash_unknown_tool(project_workdir: Path) -> None:
    build_dir = project_workdir / "build"
    build_dir.mkdir()
    (build_dir / "firmware.bin").write_bytes(b"\x00")
    result = runner.invoke(app, ["flash", "--tool", "openocd"])
    assert result.exit_code == 1


def test_flash_stlink_invokes_st_flash(
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    build_dir = project_workdir / "build"
    build_dir.mkdir()
    (build_dir / "firmware.bin").write_bytes(b"\x00")
    calls: list[list[str]] = []

    def fake_run(cmd: Any, **_kwargs: Any) -> Any:
        calls.append(list(cmd))

        class _R:
            returncode = 0

        return _R()

    monkeypatch.setattr("stmtool.cli.subprocess.run", fake_run)
    result = runner.invoke(app, ["flash", "--tool", "stlink"])
    assert result.exit_code == 0
    assert any(c[0] == "st-flash" for c in calls)


def test_completion_prints_script(monkeypatch: pytest.MonkeyPatch) -> None:
    class _R:
        returncode = 0
        stdout = "_stmtool_complete() { ... }\n"
        stderr = ""

    monkeypatch.setattr("stmtool.cli.subprocess.run", lambda *a, **kw: _R())
    result = runner.invoke(app, ["completion", "bash"])
    assert result.exit_code == 0
    assert "_stmtool_complete" in result.stdout


def test_completion_handles_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    class _R:
        returncode = 1
        stdout = ""
        stderr = "unknown shell"

    monkeypatch.setattr("stmtool.cli.subprocess.run", lambda *a, **kw: _R())
    result = runner.invoke(app, ["completion", "tcsh"])
    assert result.exit_code == 1
