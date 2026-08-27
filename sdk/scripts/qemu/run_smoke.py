#!/usr/bin/env python3
"""Run a firmware image under QEMU and assert it reached the expected state.

The runtime smoke gate from #95. Host unit tests cover the CMSIS-free modules
and -Werror covers compilation, but neither catches "links fine, hangs on the
board": a broken vector table, a stack that never starts, a work queue that
deadlocks. QEMU catches exactly that class.

Scope is deliberately narrow. QEMU's netduinoplus2 machine implements USART,
SPI, EXTI, timers, ADC and RCC, but *not* GPIO, I2C, DMA, CRC or the flash
controller, so this is a gate on the runtime and the concurrency layer -- not
a substitute for hardware. See docs/emulation.md.

Usage:
    run_smoke.py --elf out/app.elf --expect "all components started"
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_MACHINE = "netduinoplus2"
# stm32f405_soc wires serial_hd(i) to USART1, USART2, USART3, UART4, ...
# The SDK templates log on USART2, which is index 1.
DEFAULT_SERIAL_INDEX = 1


def build_command(
    elf: Path,
    machine: str,
    serial_index: int,
    trace_log: Path | None,
) -> list[str]:
    # -display none rather than -nographic: -nographic implicitly multiplexes
    # the monitor onto the first serial port, which both collides with an
    # explicit -serial stdio and prints a monitor banner into the output we
    # are about to grep.
    cmd = ["qemu-system-arm", "-M", machine, "-display", "none", "-monitor", "none"]
    # Every serial slot before the one we care about is discarded, so the
    # interesting UART lands on stdout.
    for _ in range(serial_index):
        cmd += ["-serial", "null"]
    cmd += ["-serial", "stdio"]
    if trace_log is not None:
        # -D keeps the unimplemented-device log out of stdout, so the serial
        # stream stays clean. That log is the machine-readable answer to
        # "which peripherals does the image actually touch".
        cmd += ["-d", "guest_errors,unimp", "-D", str(trace_log)]
    cmd += ["-kernel", str(elf)]
    return cmd


def run(cmd: list[str], timeout: int) -> str:
    """Run QEMU and return whatever it printed before the timeout.

    The firmware never exits on its own -- it is a super-loop -- so the
    timeout is the normal path, not a failure.
    """
    with subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    ) as proc:
        try:
            output, _ = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            output, _ = proc.communicate()
    return output or ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--machine", default=DEFAULT_MACHINE)
    parser.add_argument("--serial-index", type=int, default=DEFAULT_SERIAL_INDEX)
    parser.add_argument(
        "--timeout",
        type=int,
        default=20,
        help="seconds to let the image run before reading its output",
    )
    parser.add_argument(
        "--expect",
        action="append",
        default=[],
        metavar="TEXT",
        help="substring that must appear in the serial output (repeatable)",
    )
    parser.add_argument(
        "--reject",
        action="append",
        default=[],
        metavar="TEXT",
        help="substring that must NOT appear (repeatable)",
    )
    parser.add_argument(
        "--trace-log",
        type=Path,
        default=None,
        help="write QEMU's guest_errors/unimp log here",
    )
    args = parser.parse_args()

    if shutil.which("qemu-system-arm") is None:
        print("qemu-system-arm not found in PATH", file=sys.stderr)
        return 2
    if not args.elf.is_file():
        print(f"no such image: {args.elf}", file=sys.stderr)
        return 2

    cmd = build_command(
        args.elf, args.machine, args.serial_index, args.trace_log
    )
    print("$ " + " ".join(cmd), flush=True)
    output = run(cmd, args.timeout)

    print("--- serial output ---")
    print(output, end="" if output.endswith("\n") else "\n")
    print("--- end of serial output ---")

    failures: list[str] = []
    for needle in args.expect:
        if needle not in output:
            failures.append(f"expected but missing: {needle!r}")
    for needle in args.reject:
        if needle in output:
            failures.append(f"present but forbidden: {needle!r}")

    if not output.strip():
        failures.append(
            "the image produced no serial output at all "
            f"(wrong --serial-index for this machine? currently {args.serial_index})"
        )

    if failures:
        print("\nSMOKE FAILED:", file=sys.stderr)
        for line in failures:
            print(f"  {line}", file=sys.stderr)
        return 1

    print("\nSMOKE OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
