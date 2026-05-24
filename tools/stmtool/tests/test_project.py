"""Tests for ``stmtool.project`` template instantiation."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest

from stmtool.project import (
    _is_sdk_root,
    create_project,
    discover_template,
    list_templates,
    resolve_sdk_root,
)


def test_is_sdk_root_requires_sdk_and_templates(tmp_path: Path) -> None:
    assert not _is_sdk_root(tmp_path)
    (tmp_path / "sdk").mkdir()
    assert not _is_sdk_root(tmp_path)
    (tmp_path / "templates").mkdir()
    assert _is_sdk_root(tmp_path)


def test_list_templates_reads_metadata(tmp_template_dir: Path) -> None:
    sdk_root = tmp_template_dir.parents[2]
    items = list_templates(sdk_root)
    assert len(items) == 1
    assert items[0] == {
        "name": "blink",
        "description": "LED blink demo",
        "category": "bare-metal",
    }


def test_list_templates_empty_dir(tmp_sdk_root: Path) -> None:
    assert list_templates(tmp_sdk_root) == []


def test_discover_template_returns_path(tmp_template_dir: Path) -> None:
    sdk_root = tmp_template_dir.parents[2]
    found = discover_template(sdk_root, "blink")
    assert found == tmp_template_dir


def test_discover_template_unknown_raises(tmp_template_dir: Path) -> None:
    sdk_root = tmp_template_dir.parents[2]
    with pytest.raises(RuntimeError, match="nonexistent"):
        discover_template(sdk_root, "nonexistent")


def test_resolve_sdk_root_honors_env(stmsdk_env: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    assert resolve_sdk_root() == stmsdk_env


def _silence_git(monkeypatch: pytest.MonkeyPatch) -> None:
    """Prevent ``git init`` in create_project from spawning a real subprocess."""

    def fake_run(*args: Any, **kwargs: Any) -> Any:
        class _Result:
            returncode = 0

        return _Result()

    monkeypatch.setattr("stmtool.project.subprocess.run", fake_run)


def test_create_project_substitutes_tokens(
    tmp_template_dir: Path,
    stmsdk_env: Path,
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _silence_git(monkeypatch)
    out = create_project("demo", "STM32F407VG", "blink")
    assert out == project_workdir / "demo"
    cmakefile = out / "CMakeLists.txt"
    assert cmakefile.exists()
    content = cmakefile.read_text()
    assert "project(demo)" in content
    assert "STM32_CHIP STM32F407VG" in content
    assert "SDK_VERSION develop" in content


def test_create_project_copies_cpp_into_src(
    tmp_template_dir: Path,
    stmsdk_env: Path,
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _silence_git(monkeypatch)
    out = create_project("demo", "STM32F407VG", "blink")
    assert (out / "src" / "main.cpp").read_text().startswith("int main()")


def test_create_project_rejects_invalid_chip(
    tmp_template_dir: Path,
    stmsdk_env: Path,
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _silence_git(monkeypatch)
    with pytest.raises(ValueError, match="BAD_CHIP"):
        create_project("demo", "BAD_CHIP", "blink")


def test_create_project_refuses_existing_dir(
    tmp_template_dir: Path,
    stmsdk_env: Path,
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _silence_git(monkeypatch)
    (project_workdir / "demo").mkdir()
    with pytest.raises(FileExistsError):
        create_project("demo", "STM32F407VG", "blink")


def test_create_project_with_claude_skipped_when_template_missing(
    tmp_template_dir: Path,
    stmsdk_env: Path,
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _silence_git(monkeypatch)
    out = create_project("demo", "STM32F407VG", "blink", with_claude=True)
    assert not (out / "CLAUDE.md").exists()


def test_create_project_with_claude_emits_file(
    tmp_template_dir: Path,
    stmsdk_env: Path,
    project_workdir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    (tmp_template_dir / "CLAUDE.md.template").write_text("# @PROJECT_NAME@ for @STM32_CHIP@\n")
    _silence_git(monkeypatch)
    out = create_project("demo", "STM32F407VG", "blink", with_claude=True)
    claude = out / "CLAUDE.md"
    assert claude.exists()
    assert "demo for STM32F407VG" in claude.read_text()
