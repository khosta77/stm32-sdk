# Quick start

## Prerequisites

| Tool | Minimum version | Purpose |
|------|-----------------|---------|
| `docker` | any recent | Runs every build; ships the pinned toolchain + FreeRTOS |
| `python` | 3.10 | Runs `stmtool` |
| `pipx` | any | Isolated install of `stmtool` (recommended) |
| `st-flash` | any | Optional, for flashing via ST-Link |

Builds happen inside the SDK Docker image, so a host `arm-none-eabi-gcc` is not
required. The image pins `arm-none-eabi-gcc` 15.2 (GCC >= 14 is mandatory for
C++20 module scanning). If you do keep a local toolchain, `stmtool doctor` warns
when it is older than GCC 14.

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

Alternatively, install `stmtool` directly from its repository:

```bash
pipx install git+https://github.com/khosta77/stmtool.git
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
  CMakeLists.txt      # CMake configuration (sources + libraries only)
  stmproject.toml     # Chip and SDK version pin
  .config             # Firmware configuration (Kconfig), from the template's defconfig
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

## Configure (optional)

Firmware content — subsystem gates, logging, FreeRTOS tunables — lives in the
project `.config` file (Kconfig). The template's `defconfig` already gives a
working `.config`, so this step is optional. To change anything, open the
menuconfig TUI:

```bash
stmtool config
```

Commit `.config` to git — it is part of the project. See
[Configuration](configuration.md) for the full option list.

## Build

Builds run inside the SDK Docker image; artifacts land in `out/`:

```bash
stmtool build
```

## Test

Run the SDK host unit tests (portable logic on the mock buses, no hardware):

```bash
stmtool test
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
