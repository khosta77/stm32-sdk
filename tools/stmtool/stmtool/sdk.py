"""SDK cache management.

Wraps the cache directory (``~/.stmtool/stm32-sdk/``) and exposes the
operations needed by the ``stmtool sdk`` subcommand: update to a tag or
``develop``, list available tags, and report the resolved path.

Reuses the lower-level helpers in ``stmtool.project`` so there is only
one implementation of ``git clone`` / ``git fetch`` for the cache.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from stmtool.project import (
    _SDK_CACHE_DIR,
    _checkout_version,
    _clone_sdk_cache,
    resolve_sdk_root,
)

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib


def _ensure_cache() -> Path:
    """Return the SDK cache path, cloning it from origin if missing."""
    if not _SDK_CACHE_DIR.is_dir():
        _clone_sdk_cache()
    return _SDK_CACHE_DIR


def update_cache(version: str) -> Path:
    """Fetch and check out ``version`` in the SDK cache.

    ``version`` may be ``"develop"`` or a release identifier like ``"0.1.2"``;
    the latter is translated to the ``v0.1.2`` tag by ``_checkout_version``.
    """
    cache = _ensure_cache()
    _checkout_version(cache, version)
    return cache


def list_versions() -> list[str]:
    """Return all release tags in the cache, newest first."""
    cache = _ensure_cache()
    result = subprocess.run(
        ["git", "-C", str(cache), "tag", "-l", "v*", "--sort=-v:refname"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def current_version() -> str:
    """Return ``git describe`` output for the cache HEAD (best-effort)."""
    cache = _ensure_cache()
    result = subprocess.run(
        ["git", "-C", str(cache), "describe", "--tags", "--always"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def resolve_path() -> Path:
    """Resolve the SDK root using the same logic as the rest of stmtool."""
    return resolve_sdk_root()


def project_sdk_version(cwd: Path | None = None) -> str | None:
    """Read ``[sdk].version`` from ``stmproject.toml`` in ``cwd`` if present."""
    cwd = cwd or Path.cwd()
    config_path = cwd / "stmproject.toml"
    if not config_path.is_file():
        return None
    try:
        with open(config_path, "rb") as f:
            config = tomllib.load(f)
    except (OSError, tomllib.TOMLDecodeError):
        return None
    version = config.get("sdk", {}).get("version")
    return version if isinstance(version, str) else None
