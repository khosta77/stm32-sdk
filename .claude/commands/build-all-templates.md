---
description: Build every template under STM32F407VG (mirrors build.yml)
---

Iterate through every template in `templates/` and try to build it
locally against `STM32F407VG`. Mirrors the GitHub Actions matrix from
`.github/workflows/build.yml`.

For each template path (relative to the repository root):

- `bare-metal/blink`
- `bare-metal/i2c-scan`
- `freertos/blink`
- `freertos/mpu6050-uart`
- `freertos/oled-display-test`
- `freertos/w25q32-flash-test`
- `freertos/imu-flash-oled-demo`

Run:

```bash
TEMPLATE="<path>"
SLUG="$(echo "$TEMPLATE" | tr '/' '-')"
rm -rf /tmp/ci-$SLUG
STMSDK_PATH="$(pwd)" stmtool project create "/tmp/ci-$SLUG" --chip STM32F407VG --template "$TEMPLATE"
( cd "/tmp/ci-$SLUG" && STMSDK_PATH="$REPO_ROOT" stmtool build --native )
```

Continue past failures so you can report the full pass/fail matrix.
At the end, print a table of `<template>: OK / FAIL`.

If any template fails, surface the exact compiler error -- do NOT
disable warning flags or add `-Wno-*` to make the failure disappear
(see CLAUDE.md). Fix the source, or ask the user.
