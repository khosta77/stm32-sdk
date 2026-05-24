"""TOML loader for the project-local ``stmproject.toml`` file."""

import sys
from pathlib import Path
from typing import Any, cast

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib


def load_config(path: Path = Path("stmproject.toml")) -> dict[str, Any]:
    """Load and return the parsed contents of an ``stmproject.toml`` file."""
    with open(path, "rb") as f:
        return cast(dict[str, Any], tomllib.load(f))
