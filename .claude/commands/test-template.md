---
description: Build a single template under STM32F407VG
argument-hint: <template-path> (e.g. bare-metal/blink)
---

Generate and build one specific template against `STM32F407VG`. Use
the argument supplied as the template name from `[template].name`
in template.toml (e.g. `i2c-scan`, `freertos-blink`, `mpu6050-uart`).

```bash
TEMPLATE="$ARGUMENTS"
REPO_ROOT="$(pwd)"
rm -rf /tmp/test-$TEMPLATE
( cd /tmp && STMSDK_PATH="$REPO_ROOT" stmtool project create "test-$TEMPLATE" --chip STM32F407VG --template "$TEMPLATE" )
cd "/tmp/test-$TEMPLATE"
STMSDK_PATH="$REPO_ROOT" stmtool build
arm-none-eabi-size out/*.elf
```

Report whether the build succeeded and the final flash/RAM size from
`arm-none-eabi-size`. On failure, surface the first compiler error.
Do not suppress warnings to force a green build (see CLAUDE.md
quality-enforcement rule).
