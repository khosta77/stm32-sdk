---
description: Run the full stmtool quality pipeline (poe ci)
---

Execute the composite `ci` task for the Python CLI:

```bash
cd tools/stmtool && poetry run poe ci
```

This runs:
- ruff check / flake8 / pylint
- ruff format / black / isort (check-only)
- mypy (strict)
- bandit (-ll severity floor)
- pytest with --cov-fail-under=70

Reporting rules:

- If every step passes, report the test count and coverage percentage.
- If any step fails, STOP and report which step failed plus the first
  few diagnostics. Per the project's CLAUDE.md you MUST NOT disable
  lint rules, add `# noqa` / `# type: ignore`, or lower thresholds on
  your own to make the failure go away. Ask the user whether to fix
  every offence or to disable the specific rule -- do not decide
  unilaterally.
