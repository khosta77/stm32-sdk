---
description: Run all stmtool auto-fixers (poe fix)
---

Apply the composite auto-fixer for the Python CLI:

```bash
cd tools/stmtool && poetry run poe fix
```

Runs `ruff check --fix`, `isort`, `black`, and `ruff format` in sequence
on both `stmtool/` and `tests/`.

After running, follow up with `/ci` (or `poetry run poe ci`) to confirm
the codebase still passes the full pipeline -- auto-fix output that
passes one step can still leave the suite red on mypy, pylint, or
bandit. Do not declare success until `poe ci` is green.
