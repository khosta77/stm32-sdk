# Quick start

## Prerequisites

| Tool | Minimum version | Purpose |
|------|-----------------|---------|
| `arm-none-eabi-gcc` | 14 | Cross-compiler for Cortex-M (14+ required for C++20 module scanning) |
| `cmake` | 3.28 | C++20 modules support |
| `python` | 3.10 | Runs `stmtool` |
| `pipx` | any | Isolated install of `stmtool` (recommended) |
| `st-flash` | any | Optional, for flashing via ST-Link |

Verify your toolchain after installing:

```bash
stmtool doctor
```

## Install `stmtool`

The preferred way uses the bundled `install.sh` script. It removes any previous
installation, clears the SDK cache, fetches the latest released tag, and sets up
shell completion:

```bash
./install.sh
```

Alternatively, install directly from a clone of this repo:

```bash
pip install ./tools/stmtool
```

## Create your first project

```bash
stmtool project create my-blink --chip STM32F407VG
cd my-blink
```

The generated layout:

```
my-blink/
  src/main.cpp        # Application entry
  CMakeLists.txt      # CMake configuration
  stmproject.toml     # Chip and SDK version pin
  .gitignore
```

Pick a different template — by default `--template blink` (bare-metal) is used.
To list all templates:

```bash
stmtool project templates
```

To start from a FreeRTOS sample (e.g. MPU6050 + UART DMA):

```bash
stmtool project create imu --chip STM32F407VG --template mpu6050-uart
```

To also generate a `CLAUDE.md` tailored to the template (with pinout, expected
serial output, verification steps for that specific scenario):

```bash
stmtool project create imu --chip STM32F407VG \
  --template mpu6050-uart --with-claude
```

## Build

Locally (uses the host's `arm-none-eabi-gcc` and `cmake`):

```bash
stmtool build --native
```

Or via Docker (no host toolchain required):

```bash
stmtool build
```

## Flash

```bash
stmtool flash
```

The default is `st-flash`. Override with `--tool` if you use a different
programmer.

## Update the SDK later

Each project pins its SDK version in `stmproject.toml`:

```toml
[sdk]
version = "develop"   # or a tag like "0.1.2"
```

To refresh the SDK cache:

```bash
stmtool sdk update                       # use [sdk] version
stmtool sdk update --version 0.1.2       # explicit tag
stmtool sdk list-versions                # see what's available
```

## Next steps

- Read the [stmtool reference](stmtool.md) for every command.
- Browse [drivers](modules/drivers.md) and [sensors](modules/sensors.md) APIs.
- Check [upgrade notes](migration.md) before bumping the SDK version.
