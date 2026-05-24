---
description: Pre-release readiness checklist for stm32-sdk
---

Verify that the current branch is ready to merge into `develop` and to
be tagged as the next release. Walk through each step and report
PASS / FAIL with details.

1. **Working tree clean.** `git status --porcelain` must be empty.
2. **Recent commits look right.** `git log --oneline develop..HEAD`
   -- summarise what is on this branch that develop does not have.
3. **stmtool quality.** `cd tools/stmtool && poetry run poe ci` --
   must be all green, coverage above 70%.
4. **Documentation builds.** `mkdocs build --strict` -- no broken
   internal links, no warnings.
5. **CI status on the latest commit.** `gh pr checks` (if a PR
   exists) or `gh run list --branch $(git branch --show-current)`
   -- confirm `build.yml`, `stmtool.yml`, `docs.yml` are all green.
6. **CHANGELOG / release notes.** Verify that `docs/release.md` and
   `docs/release.ru.md` have a section for the upcoming version.
7. **Tag plan.** Determine the next tag (vMAJOR.MINOR.PATCH) and
   confirm it does not already exist with `git tag --list`.

Do NOT create the tag yourself. Report the readiness summary and let
the maintainer cut the release.
