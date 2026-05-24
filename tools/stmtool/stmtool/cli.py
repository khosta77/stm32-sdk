"""Top-level Typer application that wires together the ``stmtool`` CLI commands."""

import os
import shutil
import subprocess
from pathlib import Path

import typer
from rich.console import Console
from rich.table import Table

from stmtool import __version__
from stmtool import sdk as sdk_module
from stmtool.completions import complete_chip, complete_flash_tool, complete_template
from stmtool.config import load_config
from stmtool.i18n import t
from stmtool.project import create_project, list_templates, resolve_sdk_root

app = typer.Typer(name="stmtool", help=t("app_help"), no_args_is_help=True)
project_app = typer.Typer(help=t("project_help"))
app.add_typer(project_app, name="project")

sdk_app = typer.Typer(help=t("sdk_help"))
app.add_typer(sdk_app, name="sdk")

console = Console()

DOCKER_IMAGE = "ghcr.io/khosta77/stm32-sdk-build:latest"


@project_app.command("create", help=t("project_create_help"))
def project_create(
    name: str = typer.Argument(..., help=t("project_name_help")),
    chip: str = typer.Option(..., "--chip", help=t("chip_help"), autocompletion=complete_chip),
    template: str = typer.Option(
        "blink", "--template", help=t("template_help"), autocompletion=complete_template
    ),
    with_claude: bool = typer.Option(False, "--with-claude", help=t("create_claude_help")),
) -> None:
    """Generate a new STM32 project directory from a template."""
    try:
        with console.status(t("initializing_git")):
            path = create_project(name, chip, template, with_claude=with_claude)
        console.print(f"[bold green]{t('project_created', name=name, path=str(path))}[/bold green]")
        if with_claude:
            if (path / "CLAUDE.md").exists():
                console.print(f"[green]{t('create_claude_added')}[/green]")
            else:
                console.print(f"[yellow]{t('create_claude_missing', template=template)}[/yellow]")
    except (RuntimeError, ValueError, FileExistsError) as e:
        console.print(f"[red]{e}[/red]")
        raise typer.Exit(code=1) from e


@project_app.command("templates", help="Show available project templates")
def project_templates() -> None:
    """Print a table of all templates available in the SDK."""
    try:
        sdk_root = resolve_sdk_root()
    except RuntimeError as e:
        console.print(f"[red]{e}[/red]")
        raise typer.Exit(code=1) from e

    templates = list_templates(sdk_root)
    if not templates:
        console.print("[yellow]No templates found[/yellow]")
        raise typer.Exit(code=1)

    table = Table(title="Available templates")
    table.add_column("Name", style="bold")
    table.add_column("Category")
    table.add_column("Description")

    for tpl in templates:
        table.add_row(tpl["name"], tpl["category"], tpl["description"])

    console.print(table)


def _resolve_target_chip(chip: str | None, config: dict[str, object]) -> str:
    """Pick the chip name from the CLI flag, the project config, or fail loudly."""
    target_chip = chip
    if not target_chip:
        target = config.get("target")
        if isinstance(target, dict):
            target_chip = target.get("chip")
    if not target_chip:
        console.print(f"[red]{t('no_chip')}[/red]")
        raise typer.Exit(code=1)
    return target_chip


def _build_native(
    target_chip: str, sdk_dir: Path, build_type: str, verbose: bool
) -> subprocess.CompletedProcess[bytes]:
    """Run ``cmake`` configure + build locally without Docker."""
    configure_cmd: list[str] = [
        "cmake",
        "-G",
        "Ninja",
        "-B",
        "build",
        f"-DSTM32_CHIP={target_chip}",
        f"-DSTM32_SDK={sdk_dir}",
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    build_cmd: list[str] = ["cmake", "--build", "build"]
    if verbose:
        build_cmd.append("--verbose")
    msg = t("building", chip=target_chip, build_type=build_type, mode=t("mode_local"))
    console.print(f"[bold green]{msg}[/bold green]")
    result = subprocess.run(configure_cmd, check=False)
    if result.returncode == 0:
        result = subprocess.run(build_cmd, check=False)
    return result


def _build_docker(
    target_chip: str, sdk_root: Path, build_type: str, verbose_flag: str
) -> subprocess.CompletedProcess[bytes]:
    """Run ``cmake`` configure + build inside the SDK Docker image."""
    cmake_cmd = (
        f"cmake -B build -DSTM32_CHIP={target_chip} -DSTM32_SDK=/sdk-repo/sdk "
        f"-DCMAKE_BUILD_TYPE={build_type} && cmake --build build {verbose_flag}"
    )
    docker_cmd: list[str] = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{Path.cwd()}:/workspace",
        "-v",
        f"{sdk_root}:/sdk-repo:ro",
        "-w",
        "/workspace",
        DOCKER_IMAGE,
        "bash",
        "-c",
        cmake_cmd,
    ]
    msg = t("building", chip=target_chip, build_type=build_type, mode=t("mode_docker"))
    console.print(f"[bold green]{msg}[/bold green]")
    return subprocess.run(docker_cmd, check=False)


@app.command(help=t("build_help"))
def build(
    release: bool = typer.Option(False, "--release", help=t("build_release")),
    native: bool = typer.Option(False, "--native", help=t("build_native")),
    verbose: bool = typer.Option(False, "--verbose", "-v", help=t("build_verbose")),
    chip: str = typer.Option(None, "--chip", help=t("build_chip"), autocompletion=complete_chip),
    clean: bool = typer.Option(False, "--clean", help=t("build_clean")),
) -> None:
    """Configure and build the current project (Docker by default)."""
    config: dict[str, object] = {}
    config_path = Path("stmproject.toml")
    if config_path.exists():
        config = load_config(config_path)

    if clean:
        console.print(f"[yellow]{t('cleaning')}[/yellow]")
        shutil.rmtree("build", ignore_errors=True)

    sdk_section = config.get("sdk")
    sdk_version = (
        sdk_section.get("version", "develop") if isinstance(sdk_section, dict) else "develop"
    )

    try:
        sdk_root = resolve_sdk_root(version=sdk_version)
    except RuntimeError as e:
        console.print(f"[red]{e}[/red]")
        raise typer.Exit(code=1) from e

    target_chip = _resolve_target_chip(chip, config)
    build_type = "Release" if release else "Debug"
    verbose_flag = "--verbose" if verbose else ""

    if native:
        result = _build_native(target_chip, sdk_root / "sdk", build_type, verbose)
    else:
        result = _build_docker(target_chip, sdk_root, build_type, verbose_flag)

    raise typer.Exit(code=result.returncode)


@app.command(help=t("flash_help"))
def flash(
    tool: str = typer.Option(
        None, "--tool", help=t("flash_tool"), autocompletion=complete_flash_tool
    ),
    verify: bool = typer.Option(False, "--verify", help=t("flash_verify")),
    erase: bool = typer.Option(False, "--erase", help=t("flash_erase")),
) -> None:
    """Flash the most recently built ``.bin`` to the connected board."""
    config: dict[str, object] = {}
    config_path = Path("stmproject.toml")
    if config_path.exists():
        config = load_config(config_path)

    flash_section = config.get("flash")
    flash_tool = tool or (
        flash_section.get("tool", "stlink") if isinstance(flash_section, dict) else "stlink"
    )

    bin_files = list(Path("build").glob("*.bin"))
    if not bin_files:
        console.print(f"[red]{t('no_bin')}[/red]")
        raise typer.Exit(code=1)

    bin_path = bin_files[0]

    if flash_tool != "stlink":
        console.print(f"[yellow]{t('not_implemented')}[/yellow]")
        raise typer.Exit(code=1)

    cmd = ["st-flash"]
    if erase:
        subprocess.run(["st-flash", "erase"], check=False)
    cmd.extend(["--reset", "write", str(bin_path), "0x08000000"])
    if verify:
        cmd.append("--verify")
    console.print(f"[bold green]{t('flashing', name=bin_path.name)}[/bold green]")
    result = subprocess.run(cmd, check=False)
    raise typer.Exit(code=result.returncode)


@sdk_app.command("update", help=t("sdk_update_help"))
def sdk_update(
    target: str = typer.Option(None, "--version", help=t("sdk_update_version_help")),
) -> None:
    """Update the cached SDK to a release tag or ``develop``."""
    target_version = target or sdk_module.project_sdk_version() or "develop"
    try:
        with console.status(t("sdk_updating", version=target_version)):
            sdk_root = sdk_module.update_cache(target_version)
    except RuntimeError as e:
        console.print(f"[red]{e}[/red]")
        raise typer.Exit(code=1) from e
    console.print(
        f"[bold green]{t('sdk_updated', version=target_version, path=str(sdk_root))}[/bold green]"
    )


@sdk_app.command("list-versions", help=t("sdk_list_versions_help"))
def sdk_list_versions() -> None:
    """Print every release tag known to the cached SDK repository."""
    versions = sdk_module.list_versions()
    if not versions:
        console.print(f"[yellow]{t('sdk_no_tags_found')}[/yellow]")
        raise typer.Exit(code=1)
    current = sdk_module.current_version()
    for v in versions:
        marker = " *" if current and (current == v or current.startswith(v + "-")) else ""
        print(f"{v}{marker}")


@sdk_app.command("path", help=t("sdk_path_help"))
def sdk_path_cmd() -> None:
    """Print the resolved SDK root path."""
    try:
        print(sdk_module.resolve_path())
    except RuntimeError as e:
        console.print(f"[red]{e}[/red]")
        raise typer.Exit(code=1) from e


@app.command(help=t("doctor_help"))
def doctor() -> None:
    """Probe the host environment for required build/flash tools."""
    table = Table(title="stmtool doctor")
    table.add_column("Component", style="bold")
    table.add_column("Status")
    table.add_column("Details")

    checks = [
        ("Docker", ["docker", "--version"]),
        ("arm-none-eabi-gcc", ["arm-none-eabi-gcc", "--version"]),
        ("CMake", ["cmake", "--version"]),
        ("st-flash", ["st-flash", "--version"]),
    ]

    for name, cmd in checks:
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=5, check=False)
            version_line = (
                result.stdout.split("\n")[0].strip()
                if result.returncode == 0
                else result.stderr.split("\n")[0].strip()
            )
            if result.returncode == 0:
                table.add_row(name, "[green]OK[/green]", version_line)
            else:
                table.add_row(name, "[red]ERROR[/red]", version_line)
        except FileNotFoundError:
            table.add_row(name, "[red]NOT FOUND[/red]", f"Install {name}")
        except subprocess.TimeoutExpired:
            table.add_row(name, "[yellow]TIMEOUT[/yellow]", "")

    console.print(table)


@app.command(help=t("completion_help"))
def completion(
    shell: str = typer.Argument("zsh", help=t("completion_shell_help")),
) -> None:
    """Print the shell completion script for ``bash``, ``zsh``, or ``fish``."""
    env = os.environ.copy()
    env["_STMTOOL_COMPLETE"] = f"source_{shell}"
    result = subprocess.run(
        ["stmtool"],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0 and result.stdout.strip():
        console.print(f"[dim]# {t('completion_hint')}[/dim]")
        print(result.stdout)
    else:
        console.print(f"[red]{result.stdout or result.stderr}[/red]")
        raise typer.Exit(code=1)


@app.command(help=t("version_help"))
def show_version() -> None:
    """Print the installed stmtool version."""
    console.print(f"stmtool {__version__}")


if __name__ == "__main__":
    app()
