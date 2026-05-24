---
description: Build a single template under STM32F407VG
argument-hint: <template-path> (e.g. bare-metal/blink)
---

Generate and build one specific template against `STM32F407VG`. Use
the argument supplied as the template path (categories/name, e.g.
`bare-metal/i2c-scan` or `freertos/oled-display-test`).

```bash
TEMPLATE="$ARGUMENTS"
SLUG="$(echo "$TEMPLATE" | tr '/' '-')"
rm -rf /tmp/test-$SLUG
STMSDK_PATH="$(pwd)" stmtool project create "/tmp/test-$SLUG" --chip STM32F407VG --template "$TEMPLATE"
cd "/tmp/test-$SLUG"
STMSDK_PATH="$REPO_ROOT" stmtool build --native
arm-none-eabi-size build/*.elf
```

Report whether the build succeeded and the final flash/RAM size from
`arm-none-eabi-size`. On failure, surface the first compiler error.
Do not suppress warnings to force a green build (see CLAUDE.md
quality-enforcement rule).
