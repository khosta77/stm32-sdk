---
description: Build a single template under STM32F407VG
argument-hint: <template-path> (e.g. bare-metal/blink)
---

Generate and build one specific template against `STM32F407VG`. Use
the argument supplied as the template name from `[template].name`
in template.toml (e.g. `i2c-scan`, `freertos-blink`, `mpu6050-uart`).

```bash
TEMPLATE="$ARGUMENTS"
rm -rf /tmp/test-$TEMPLATE
STMSDK_PATH="$(pwd)" stmtool project create "/tmp/test-$TEMPLATE" --chip STM32F407VG --template "$TEMPLATE"
cd "/tmp/test-$TEMPLATE"
STMSDK_PATH="$REPO_ROOT" stmtool build --native
arm-none-eabi-size build/*.elf
```

Report whether the build succeeded and the final flash/RAM size from
`arm-none-eabi-size`. On failure, surface the first compiler error.
Do not suppress warnings to force a green build (see CLAUDE.md
quality-enforcement rule).
