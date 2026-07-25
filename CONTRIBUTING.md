# Contributing to stm32-sdk

Thanks for helping out. This guide covers the mechanics; the design rules live
in [`CLAUDE.md`](CLAUDE.md) and the docs under [`docs/`](docs/).

## One-time setup

Install the git hooks (they are mandatory — CI enforces the same checks):

```bash
pip install pre-commit
pre-commit install --hook-type pre-commit --hook-type commit-msg
```

This wires two gates from [`.pre-commit-config.yaml`](.pre-commit-config.yaml):

- **clang-format** (pinned to 19.1.3) on every staged C / C++ / `.cppm` file.
- **Conventional Commits** lint on the commit message.

`stmtool` lives in its own repository ([`khosta77/stmtool`](https://github.com/khosta77/stmtool))
since v0.2.1; its `poe ci` gate runs there, not here.

Use clang-format 19 locally so your output matches the pinned version.

## Code style

- C / C++: Google style, 2-space indent, 80-column limit, braced-init lists as
  `{nullptr}` — all encoded in [`.clang-format`](.clang-format). Run
  `clang-format -i <files>` or let the pre-commit hook fix them.
- Vendor CMSIS / newlib sources are excluded via
  [`.clang-format-ignore`](.clang-format-ignore) and must stay pristine so they
  remain diffable against upstream. Do not reformat them.
- The rest of the SDK conventions (CMSIS-only, no raw MMIO, config structs with
  no defaults, concepts over vtables, ...) are in [`CLAUDE.md`](CLAUDE.md).

## Commits and pull requests

- **Commit messages: English, [Conventional Commits](https://www.conventionalcommits.org/)**
  (`feat:`, `fix:`, `style:`, `docs:`, `ci:`, `build:`, `refactor:`, ...). The
  commit-msg hook rejects anything else.
- **Pull requests target `develop`; the PR description is in Russian.** This
  split keeps history machine-readable while the PR narrative stays natural for
  the team.
- **Docs travel with the code.** Any change to a public surface (driver, sensor,
  `stmtool` CLI, or a rule) updates the matching page under `docs/` — both the
  English `*.md` and the Russian `*.ru.md` — in the same PR. See the
  "Documentation maintenance" section of `CLAUDE.md`.

## Before you push

- C++: `clang-format --dry-run --Werror` is clean (or the hook passed), and the
  templates still build under `-Werror`.
- `stmtool` changes go to the [`khosta77/stmtool`](https://github.com/khosta77/stmtool)
  repository, where `poe ci` is the gate — not this repo.

## Releases

Releases are cut from tags on `develop` with bilingual notes; the procedure is
documented in [`docs/release.md`](docs/release.md) and `CLAUDE.md`.
