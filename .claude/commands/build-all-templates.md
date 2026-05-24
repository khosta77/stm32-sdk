---
description: Build every template under STM32F407VG (mirrors build.yml)
---

Iterate through every template in `templates/` and try to build it
locally against `STM32F407VG`. Mirrors the GitHub Actions matrix from
`.github/workflows/build.yml`.

For each template name (the `[template].name` field in the
template.toml, not the directory path):

- `blink`
- `i2c-scan`
- `freertos-blink`
- `mpu6050-uart`
- `oled-display-test`
- `w25q32-flash-test`
- `imu-flash-oled-demo`

Run:

```bash
TEMPLATE="<name>"
rm -rf /tmp/ci-$TEMPLATE
STMSDK_PATH="$(pwd)" stmtool project create "/tmp/ci-$TEMPLATE" --chip STM32F407VG --template "$TEMPLATE"
( cd "/tmp/ci-$TEMPLATE" && STMSDK_PATH="$REPO_ROOT" stmtool build --native )
```

Continue past failures so you can report the full pass/fail matrix.
At the end, print a table of `<template>: OK / FAIL`.

If any template fails, surface the exact compiler error -- do NOT
disable warning flags or add `-Wno-*` to make the failure disappear
(see CLAUDE.md). Fix the source, or ask the user.
