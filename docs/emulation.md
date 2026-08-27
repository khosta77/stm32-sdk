# Emulation (QEMU)

Since v0.2.4 CI boots a firmware image under QEMU and checks that it reached a
working state. This page says what that gate does and — more importantly —
what it does **not** prove.

## What it is for

Three layers of verification exist, and they catch different things:

| Layer | Catches | Misses |
|---|---|---|
| `-Werror` build | it does not compile | anything at runtime |
| host tests (`stmtool test`) | logic in CMSIS-free modules | anything touching hardware or the RTOS |
| **QEMU smoke** | **"links fine, hangs on the board"** | **register-level peripheral behaviour** |

The gap the emulator closes is real and otherwise uncovered: a broken vector
table, a scheduler that never starts, a work queue that deadlocks, a heap that
overflows on the first allocation. Note that `system.work_queue`,
`system.executor`, `system.timer` and `system.signal_bus` pull in CMSIS and
FreeRTOS and are therefore absent from the host test suite — QEMU is currently
the only automated check they get.

## What QEMU actually emulates

The machine is `netduinoplus2` (STM32F405RG, Cortex-M4). Its SoC model
implements:

> ARMv7-M core and NVIC, RCC, SYSCFG, USART ×8, TIM2–5, ADC ×6, SPI ×6, EXTI

and explicitly does **not** implement:

> GPIO, I2C, DMA, CRC, the flash interface unit, RNG, RTC, DAC, SDIO, CAN,
> Ethernet, USB OTG, IWDG/WWDG, DCMI, I2S, PWR

Accesses to an unimplemented block are absorbed silently: reads return zero,
writes are logged. That means a GPIO-driven LED demo *runs* but shows nothing,
and an I2C scan finds nothing — the emulator is not lying, it simply has no
model to lie with.

There is **no STM32G0 machine** in QEMU, and no Cortex-M0/M0+ STM32 machine at
all. Work on emulating the G0 board therefore goes to Renode instead
(issue #94), not here.

## The gate

`sdk/scripts/qemu/run_smoke.py` boots an ELF, lets it run for a timeout —
firmware is a super-loop, so timing out is the normal path — and asserts on
substrings of its serial output:

```bash
python3 sdk/scripts/qemu/run_smoke.py \
    --elf out/my-app.elf \
    --timeout 20 \
    --expect "all components started" \
    --reject "CRITICAL"
```

Exit code 0 means every `--expect` appeared and no `--reject` did.

Two flags matter and are easy to get wrong:

- `--serial-index` (default 1). `stm32f405_soc` wires `serial_hd(i)` to
  USART1, USART2, USART3, UART4… in that order, and the SDK templates log on
  **USART2**, which is index 1. A wrong index produces an empty log, which the
  script reports as such rather than as a missing marker.
- The script uses `-display none -monitor none`, not `-nographic`.
  `-nographic` multiplexes the QEMU monitor onto the first serial port, which
  both collides with an explicit `-serial stdio` and prints a monitor banner
  into the text being matched.

CI runs it on `freertos/signal-bus-demo` built for `STM32F405RG` — the one
template whose behaviour is fully observable on this machine, since it needs
only a UART and the concurrency layer. Expected output:

```
=== signal-bus-demo bootstrap ===
firmware v0.0.0-untagged
all components started
workqueue self-check: PASS
beat #0 value=-100
```

QEMU runs on the CI runner, not inside the SDK Docker image, which stays a
lean build environment.

## The device log is generated, not maintained

`--trace-log` passes `-d guest_errors,unimp -D <file>` to QEMU, so every access
to an unimplemented block is recorded. CI keeps it as the
`qemu-unimplemented-devices` artifact. For the smoke image it contains GPIOA
and GPIOD only — the LED — which is how you can tell nothing functional is
being silently swallowed.

This is deliberately a generated answer to "which peripherals does this
firmware touch that the emulator does not model": a hand-maintained list would
be stale within one release.

## What this is not

It is not a substitute for hardware. Register-level correctness of I2C timing,
SPI framing, DMA transfers or flash erase behaviour cannot be observed here at
all — for those blocks QEMU has no model, and for the ones it does have, the
model is an approximation. Anything claiming a driver works on silicon still
means: flashed to a board and watched.
