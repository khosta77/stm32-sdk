# Release process

## Versioning policy

Releases use SemVer (`vMAJOR.MINOR.PATCH`). The project is currently pre-1.0:
patch and minor releases may both introduce small source-side changes. Where
exactly the line falls between "patch" and "minor" is at the maintainer's
discretion until the API stabilises (target: v1.0).

Practical advice for downstream projects:

- Pin a specific tag in `stmproject.toml` (`[sdk] version = "0.1.2"`) instead of
  `develop`.
- Before bumping, read the [upgrade notes](migration.md) for source-side changes.
- Use `stmtool sdk update --version <tag>` to refresh the cache in lockstep.

## Tooling

The version number is derived from git tags via `poetry-dynamic-versioning`
(switched from `setuptools-scm` in v0.1.4). There is no hand-edited version
constant anywhere — tagging the repository is what creates a release for
`stmtool` consumers.

`tools/stmtool/pyproject.toml` configures:

```toml
[build-system]
requires = ["poetry-core>=1.9", "poetry-dynamic-versioning>=1.4"]
build-backend = "poetry_dynamic_versioning.backend"

[tool.poetry-dynamic-versioning]
enable = true
vcs = "git"
style = "pep440"
```

The plugin walks up from `tools/stmtool/pyproject.toml` to find the
repository `.git` and picks up the latest `vMAJOR.MINOR.PATCH` tag —
the same source of truth the SDK CMake side already uses.

## Release procedure

1. Make sure `develop` is green on CI (build matrix on F407VG/F401CE/F411CE).
2. Locally preview docs with `mkdocs serve`.
3. Tag the merge commit on `develop`:
   ```bash
   git tag -a v0.1.3 -m "Release v0.1.3" <commit>
   git push origin v0.1.3
   ```
4. Create the GitHub Release using notes from this page's "Release history".
5. The `docs.yml` workflow rebuilds the site automatically; verify
   <https://khosta77.github.io/stm32-sdk/> picks up the new content.

## Release history

### v0.1.4 (preparing)

Focus: quality and infrastructure, no new SDK features beyond
[issue #9](https://github.com/khosta77/stm32-sdk/issues/9).

Highlights:

- `driver::NullGpioPin` — empty `IGpioPin` implementation for boards
  where the SPI CS line is hardwired (issue #9). Drop-in for
  sensors that take `IGpioPin&` such as `W25q32`.
- `stmtool` migrated from `setuptools` to Poetry + `poethepoet`. New
  `poetry run poe ci` runs ruff / flake8 / pylint / black / isort /
  mypy (strict) / bandit / pytest in one shot; `poetry run poe fix`
  applies all auto-fixers. The runtime `__version__` now comes from
  `importlib.metadata`; the generated `_version.py` is gone.
- `stmtool` gained a baseline pytest suite (~80% coverage, 50 tests)
  and a new `.github/workflows/stmtool.yml` CI workflow that runs
  `poe ci` on every PR touching `tools/stmtool/**`.
- `.github/workflows/build.yml` rewritten: the chip matrix is now
  `[STM32F407VG]` only, and each PR builds **all 7 existing templates**
  against it in parallel (`fail-fast: false`).
- `-Wall -Wextra -Wpedantic -Wshadow -Werror` are now permanently
  enabled on `stm32_core` and propagate to drivers, RTOS, sensors,
  and user code. See [build-flags](build-flags.md) for the policy.
- Project-level `.claude/commands/{ci,fix,build-all-templates,test-template,release-check}.md`
  for contributors using Claude Code.
- New `CLAUDE.md` "Quality enforcement" section: linter rules and
  `-W*` flags must NOT be disabled without explicit user approval.

Known limitations / not validated in CI:

- **STM32F401CE and STM32F411CE** remain supported by the SDK code
  but are no longer covered by CI (the maintainer has no physical
  hardware for these). Use at your own risk; if you build for them
  and hit a warning that breaks `-Werror`, please open an issue.
- `bare-metal/blink/main.cpp` was updated to use `g_ticks = g_ticks + 1`
  instead of `++g_ticks` (the C++20 deprecation of `operator++` on
  volatile). Downstream projects that copied the original may need
  the same one-line fix.

Deferred to v0.1.5+:

- All P1 `stmtool` commands from
  [issue #2](https://github.com/khosta77/stm32-sdk/issues/2):
  `monitor`, `size`, `config`, `project info`, `device list`, `chips`,
  `boards`.
- New bare-metal templates: `uart-echo`, `spi-sensor`, `adc-dma`.
- Board definitions (`.toml` files) and Python chipdb.
- `-Wconversion` warning flag (high noise on CMSIS-derived code,
  needs a separate cleanup pass).

### v0.1.3 (preparing)

Highlights:

- New documentation site (MkDocs Material), English and Russian.
- New `stmtool sdk update`, `sdk list-versions`, `sdk path` subcommands.
- Optional `CLAUDE.md` generation per-template via `--with-claude`.
- All new sensors (MPU6050, SSD1306, W25Q32) and DMA stream wrapper documented.
- I2C multi-byte read fix per RM0090 §27.3.3, valid at 400 kHz.
- `GpioConfig` rewritten as an aggregate with `consteval gpio({...})` validator.

See [upgrade notes](migration.md) for source-side changes required.

### v0.1.2

- `stmtool` install hardening (`install.sh` clears cache on reinstall).

### v0.1.1

- FreeRTOS templates (`freertos/blink`, `freertos/mpu6050-uart`) registered for
  `stmtool` discovery.

### v0.1.0

- Initial release. `setuptools-scm` versioning for `stmtool` and the SDK as a
  whole. CI moved from `main` to `develop`.

## GitHub Pages

The documentation site is published from the `gh-pages` branch (created by the
`docs.yml` workflow). The site is available at
<https://khosta77.github.io/stm32-sdk/>. After the first deployment, enable
Pages in repository Settings → Pages → Source = `gh-pages` / `(root)`.
