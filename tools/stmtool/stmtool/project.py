"""Project scaffolding and SDK-cache helpers used by the stmtool CLI."""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from stmtool.i18n import t

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

_CHIP_RE = re.compile(r"^STM32[A-Z]\d{3}[A-Z]{2}$")
_DEFAULT_REPO = "https://github.com/khosta77/stm32-sdk.git"

_GITIGNORE = """\
build/
*.o
*.d
*.elf
*.hex
*.bin
*.map
__pycache__/
.DS_Store
"""


_SDK_CACHE_DIR = Path.home() / ".stmtool" / "stm32-sdk"


def _is_sdk_root(p: Path) -> bool:
    """Return ``True`` if ``p`` contains both ``sdk/`` and ``templates/``."""
    return (p / "sdk").is_dir() and (p / "templates").is_dir()


def _clone_sdk_cache() -> Path:
    """Clone the SDK repository into the user-level cache directory."""
    print(t("sdk_cloning"), file=sys.stderr)
    _SDK_CACHE_DIR.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        ["git", "clone", _DEFAULT_REPO, str(_SDK_CACHE_DIR)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(t("sdk_not_found"))
    return _SDK_CACHE_DIR


def _checkout_version(repo_dir: Path, version: str) -> None:
    """Check out ``develop`` or a release tag in the cached SDK repository."""
    if version == "develop":
        subprocess.run(
            ["git", "-C", str(repo_dir), "fetch", "origin", "develop"],
            capture_output=True,
            check=False,
        )
        subprocess.run(
            ["git", "-C", str(repo_dir), "checkout", "develop"],
            capture_output=True,
            check=False,
        )
        subprocess.run(
            ["git", "-C", str(repo_dir), "pull", "--ff-only"],
            capture_output=True,
            check=False,
        )
    else:
        tag = f"v{version}"
        subprocess.run(
            ["git", "-C", str(repo_dir), "fetch", "origin", "tag", tag],
            capture_output=True,
            check=False,
        )
        subprocess.run(
            ["git", "-C", str(repo_dir), "checkout", tag],
            capture_output=True,
            check=False,
        )


def resolve_sdk_root(version: str = "develop") -> Path:
    """Locate the SDK root via ``STMSDK_PATH``, project ancestors, or cache."""
    env_path = os.environ.get("STMSDK_PATH")
    if env_path:
        p = Path(env_path)
        if _is_sdk_root(p):
            return p

    current = Path(__file__).resolve()
    for parent in current.parents:
        if _is_sdk_root(parent):
            return parent

    if _is_sdk_root(_SDK_CACHE_DIR):
        _checkout_version(_SDK_CACHE_DIR, version)
        return _SDK_CACHE_DIR

    _clone_sdk_cache()
    _checkout_version(_SDK_CACHE_DIR, version)
    return _SDK_CACHE_DIR


def list_templates(sdk_root: Path) -> list[dict[str, str]]:
    """Enumerate all templates under ``sdk_root/templates`` with metadata."""
    templates_dir = sdk_root / "templates"
    result: list[dict[str, str]] = []

    for category_dir in sorted(templates_dir.iterdir()):
        if not category_dir.is_dir():
            continue
        for tpl_dir in sorted(category_dir.iterdir()):
            if not tpl_dir.is_dir():
                continue
            meta_path = tpl_dir / "template.toml"
            if not meta_path.exists():
                continue
            with open(meta_path, "rb") as f:
                meta = tomllib.load(f)
            tpl = meta.get("template", {})
            result.append(
                {
                    "name": tpl.get("name", ""),
                    "description": tpl.get("description", ""),
                    "category": tpl.get("category", ""),
                }
            )

    return result


def discover_template(sdk_root: Path, template_name: str) -> Path:
    """Resolve ``template_name`` to its directory under ``sdk_root/templates``."""
    templates_dir = sdk_root / "templates"
    available: list[str] = []

    for category_dir in sorted(templates_dir.iterdir()):
        if not category_dir.is_dir():
            continue
        for tpl_dir in sorted(category_dir.iterdir()):
            if not tpl_dir.is_dir():
                continue
            meta_path = tpl_dir / "template.toml"
            if not meta_path.exists():
                continue
            with open(meta_path, "rb") as f:
                meta = tomllib.load(f)
            name = meta.get("template", {}).get("name", "")
            available.append(name)
            if name == template_name:
                return tpl_dir

    raise RuntimeError(t("template_not_found", name=template_name, available=", ".join(available)))


def create_project(
    name: str,
    chip: str,
    template_name: str,
    *,
    with_claude: bool = False,
    sdk_version: str = "develop",
) -> Path:
    """Create a new STM32 project directory from a template under the cwd."""
    sdk_root = resolve_sdk_root()
    tpl_dir = discover_template(sdk_root, template_name)

    chip = chip.upper()
    if not _CHIP_RE.match(chip):
        raise ValueError(t("invalid_chip", chip=chip))

    target = Path.cwd() / name
    if target.exists():
        raise FileExistsError(t("project_exists", name=name))

    target.mkdir()
    (target / "src").mkdir()

    for item in sorted(tpl_dir.iterdir()):
        if item.name == "template.toml":
            continue
        if item.name == "CLAUDE.md.template" and not with_claude:
            continue

        if item.suffix == ".template":
            content = item.read_text()
            content = content.replace("@PROJECT_NAME@", name)
            content = content.replace("@STM32_CHIP@", chip)
            content = content.replace("@SDK_VERSION@", sdk_version)
            dest = target / item.stem
            dest.write_text(content)
        elif item.name.endswith((".cpp", ".c", ".h")):
            shutil.copy2(item, target / "src" / item.name)
        elif item.is_dir():
            shutil.copytree(item, target / item.name)
        else:
            shutil.copy2(item, target / item.name)

    (target / ".gitignore").write_text(_GITIGNORE)
    (target / "README.md").write_text(f"# {name}\n")

    subprocess.run(["git", "init"], cwd=target, capture_output=True, check=False)

    return target
