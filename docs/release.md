# Release process

## Versioning policy

Releases use SemVer (`vMAJOR.MINOR.PATCH`). The project is currently pre-1.0:
patch and minor releases may both introduce small source-side changes. Where
exactly the line falls between "patch" and "minor" is at the maintainer's
discretion until the API stabilises (target: v1.0).

Practical advice for downstream projects:

- Pin a specific tag in `stmproject.toml` (`[sdk] version = "0.1.2"`) instead of
  `develop`.
- Before bumping, read the [upgrade notes](migration.md) for source-side changes.
- Use `stmtool sdk update --version <tag>` to refresh the cache in lockstep.

## Tooling

The version number is derived from git tags via `poetry-dynamic-versioning`
(switched from `setuptools-scm` in v0.1.4). There is no hand-edited version
constant anywhere — tagging the repository is what creates a release for
`stmtool` consumers.

`tools/stmtool/pyproject.toml` configures:

```toml
[build-system]
requires = ["poetry-core>=1.9", "poetry-dynamic-versioning>=1.4"]
build-backend = "poetry_dynamic_versioning.backend"

[tool.poetry-dynamic-versioning]
enable = true
vcs = "git"
style = "pep440"
```

The plugin walks up from `tools/stmtool/pyproject.toml` to find the
repository `.git` and picks up the latest `vMAJOR.MINOR.PATCH` tag —
the same source of truth the SDK CMake side already uses.

## Release procedure

1. Make sure `develop` is green on CI (build matrix on F407VG/F401CE/F411CE).
2. Locally preview docs with `mkdocs serve`.
3. Tag the merge commit on `develop`:
   ```bash
   git tag -a v0.1.3 -m "Release v0.1.3" <commit>
   git push origin v0.1.3
   ```
4. Create the GitHub Release using notes from this page's "Release history".
5. The `docs.yml` workflow rebuilds the site automatically; verify
   <https://khosta77.github.io/stm32-sdk/> picks up the new content.

## Release history

### v0.2.1

Focus: extract `stmtool` from this monorepo into its own repository,
[`khosta77/stmtool`](https://github.com/khosta77/stmtool), so the tool is
versioned, released, and evolved independently of the SDK (west-style
separation). No source-visible SDK changes.

Highlights:

- **Standalone tool.** `tools/stmtool/` is removed from the SDK. History was
  preserved via `git filter-repo` (blame intact). The package adopts a src-layout
  and stays SDK-content-free — it resolves an SDK checkout at runtime through
  `STMSDK_PATH` / the `~/.stmtool/stm32-sdk/` cache / a clone
  (`STMTOOL_SDK_REPO`), exactly as before.
- **Independent versioning.** `stmtool` is now `v0.N`: the major is pinned at 0,
  the minor is auto-bumped by CI on every merge to `main` (an `autotag` workflow
  pushes the next tag; `poetry-dynamic-versioning` reads it). No hand-edited
  version, no coupling to SDK tags.
- **pipx distribution.** Install with
  `pipx install git+https://github.com/khosta77/stmtool.git`; the SDK's
  `install.sh` now points there. The SDK `build.yml` installs the tool from the
  new repo; the `stmtool.yml` CI workflow moved with it.

Notes:

- Downstream action: reinstall the tool from the new repo (see the
  [upgrade notes](migration.md#new-in-v021)). The CLI and commands are unchanged;
  the version command is `stmtool show-version`.

### v0.2.0

Focus: a cross-cutting stabilization pass that closes the v0.1.x line — a
driver/sensor/framework bug audit, closing accumulated debt, and unifying MMIO
access. Opens the v0.2.x series (issue #74).

Highlights:

- **Correctness fixes.** GPIO clock enable now covers ports F–I (was A–E only);
  `SingleThreadExecutor::postFromISR` yields to a woken higher-priority task
  instead of waiting a tick; `Mpu6050::init` no longer swallows the accel/gyro
  range-write status; DMA/UART busy-waits are bounded and return
  `Status::Timeout`; UART DMA `writeNonBlocking` is genuinely non-blocking;
  `SpiDataSize::Bits16` (never implemented) is rejected at compile time; an I2C
  `BTF` timeout is reported instead of a false `Ok`.
- **MMIO unification.** Every register access in the GPIO/I2C/SPI/flash/clock
  drivers now goes through `driver::reg::*` — no raw `|=` / `=` on `volatile`
  registers remain. Behaviour is byte-identical.
- **`[[nodiscard]]` everywhere.** All value-returning driver/RTOS/util methods
  are now `[[nodiscard]]`, so ignored statuses fail the build under `-Werror`.
- **Host-test coverage.** New `ctest` executables for `CircularBuffer`, `W25q32`
  and `Ssd1306` logic on the mock buses (7 host tests total).
- **Template & tooling cleanup.** The `bare-metal/blink` template now uses the
  `GpioPin` driver instead of raw MMIO; the removed `--native` build flag is
  gone from every template `CLAUDE.md` and the repo build skills.

Notes:

- Three source-visible breaking changes: `SpiDataSize::Bits16` now fails to
  compile, the new `[[nodiscard]]` markers can trip a downstream `-Werror`, and
  `IImu::setAccelRange` / `setGyroRange` now return `driver::Status`. See the
  [upgrade notes](migration.md#new-in-v020).
- Deferred: the SDK-wide `-fno-exceptions` / `-fno-rtti` size optimization is
  tracked separately — it conflicts with the `throw`-based `consteval` config
  validators and needs a throw-free rewrite first.

### v0.1.16

Focus: completing the C→C++ migration by moving the last vendor runtime
sources — the newlib glue and the CMSIS `system_*` / vector tables — to C++,
with no ABI or behaviour change (issues #42, #43). This closes the v0.1.x line.

Highlights:

- **newlib glue → C++ (#42).** `core/src/newlib/{assert,exit,sbrk,startup,
  syscalls}.cpp` replace the former `.c` files. Every C-ABI symbol the linker
  and libc reference — `_start`, `_sbrk`, `_exit`, `abort`, `__assert_func`,
  `__initialize_args`, the libnosys/POSIX stubs — stays `extern "C"` with the
  same name; the `.after_vectors` `_start`, the `always_inline` copy/zero
  helpers, the `register char* asm("sp")` binding and the `-nostartfiles` /
  `nano.specs` contract are preserved verbatim. `main` is declared outside the
  `extern "C"` block (it is never mangled and must not have C language linkage).
- **CMSIS `system_*` / vector tables → C++ (#43).** `system_stm32f4xx.cpp` and
  the seven per-family `vectors_*.cpp` compile as C++. The vector tables,
  `Default_Handler` and every weak `alias("Default_Handler")` IRQ handler are
  wrapped in `extern "C"` so their names resolve unmangled; `__isr_vectors` is
  forced back to external linkage (a `const` array would otherwise become
  TU-local in C++) while keeping its `.isr_vector` placement. `SystemInit` /
  `SystemCoreClockUpdate` inherit C linkage from the CMSIS header. The ST
  register logic is untouched.

Notes:

- No breaking changes. Symbol names, linkage and behaviour are byte-for-byte
  identical — verified with `nm` (no mangled C-ABI symbols) and an unchanged
  flash size across all ten templates. The migrated sources are now formatted
  like the rest of the SDK; only the vendor CMSIS device headers (`stm32f4xx.h`
  and the device-header web) stay `.h` and pristine, because renaming them would
  break ST's internal cross-includes and upstream diffability. See the
  [upgrade notes](migration.md#new-in-v0116).
- Tooling: `stmtool flash --verify` no longer passes an invalid flag to
  `st-flash` 1.8.0 (which verifies every write anyway), and the vendored CMSIS
  headers are marked `linguist-vendored` so GitHub's language stats reflect the
  authored C++ instead of the ~10 MB of ARM/ST headers.

### v0.1.15

Focus: migrating the SDK's own low-level C glue to idiomatic C++, with no ABI
or behaviour change (issues #38, #39, #40, #41).

Highlights:

- **Diagnostic trace glue → C++ (#38).** `core/src/diag/trace.cpp` and
  `trace-impl.cpp` replace the former `.c` files: `constexpr` buffer size,
  `static_cast`/`reinterpret_cast`, file-local backends in an anonymous
  namespace. The portable `trace_*` helpers keep C linkage via `diag/trace.h`.
- **Hardware init/reset glue → C++ (#39).** `initialize-hardware.cpp` and
  `reset-hardware.cpp` wrap `__initialize_hardware_early` /
  `__initialize_hardware` / `__reset_hardware` in `extern "C"` so the still-C
  newlib `_start` / `_exit` resolve them unchanged; the `weak` overrides and
  CMSIS `SCB->` access are preserved verbatim.
- **Exception handlers → C++ (#40).** `exception-handlers.cpp` keeps every
  `Reset_Handler` / `HardFault_Handler` / `SysTick_Handler` … as `extern "C"`
  (the vendor vector table references them by name), with the naked assembly
  trampolines, `.after_vectors` sections and `TRACE`/`DEBUG` diagnostics intact.
- **FreeRTOS hooks → C++ (#41).** `rtos/src/freertos_hooks.cpp` keeps
  `vApplicationStackOverflowHook` and friends as `extern "C"` for the kernel;
  the static idle/timer task storage moves into an anonymous namespace.

Notes:

- No breaking changes. Symbol names and behaviour are byte-for-byte identical —
  overriding `__initialize_hardware`, `__reset_hardware` or any handler from
  application code (including a `.c` file) still works through the `extern "C"`
  + `weak` boundary. The migrated files are now formatted like the rest of the
  SDK; the vendor `system_*` / vector tables and newlib runtime stay C. See the
  [upgrade notes](migration.md#new-in-v0115).

### v0.1.14

Focus: a compile-time-filtered logging facility with swappable backends, plus a
multi-arch build image so local builds run natively on Apple Silicon (issues
#36, #37, #81).

Highlights:

- **Compile-time logging `LOG_*` (#36).** A new `driver.log` module plus the
  textual `driver/log.hpp` macro header give `LOG_ERROR`/`WARN`/`INFO`/`DEBUG`/
  `TRACE`, each in bare / `_U32` / `_HEX` forms. Two filters: the compile-time
  ceiling `STM32_LOG_LEVEL` expands stripped levels to `((void)0)` (format string
  and all — zero flash), and the runtime `driver::log::setLevel` gates the rest
  without a rebuild. Formatting is by hand, so it works under `nano.specs`.
- **Swappable backends (#37).** The sink is a captureless
  `void(*)(void*, const char*, size_t)` thunk installed via
  `driver::log::setSink`. Two ship with `install()`: `ItmBackend` (SWO/ITM port
  0, read over SWV) and `UartBackend<UartDriver>` (generic over any
  `driver::IUart`). `STM32_LOG_BACKEND` (`none`/`itm`/`uart`) names the intended
  one at build time and exposes a `STM32_LOG_BACKEND` define for generic app
  code.
- **Multi-arch build image (#81).** `docker/Dockerfile.build` and `.ci` pick the
  ARM toolchain by host architecture (`uname -m`), and `docker-image.yml`
  publishes a `linux/amd64,linux/arm64` manifest. On Apple Silicon `docker pull`
  now fetches the arm64 layer with a native `aarch64` toolchain, so
  `stmtool build` / `stmtool test` no longer crash under Rosetta. The user
  contract is unchanged — image only, no new flags.

Notes:

- No breaking changes. Logging is opt-in and ships with `STM32_USE_DRIVERS`; the
  defaults (`STM32_LOG_LEVEL=INFO`, `STM32_LOG_BACKEND=none`) add nothing until
  you install a backend. See [Logging](modules/logging.md) and the
  [upgrade notes](migration.md#new-in-v0114).
- Apple Silicon users on a cached amd64 image should `docker pull` once to
  replace it with the multi-arch manifest.

### v0.1.13

Focus: a quality / infrastructure release that makes the toolchain reproducible,
adds a real host-test path, and removes the divergent local build (issues #33,
#34, #35, #47, #79).

Highlights:

- **Docker-only builds; artifacts in `out/`.** `stmtool build --native` is gone;
  every build runs inside the SDK image, so the environment is identical
  locally and in CI. Build output moved from `build/` to `out/` (gitignored).
  The image is overridable via `STMTOOL_DOCKER_IMAGE`. **Breaking** — drop
  `--native` and repoint any `build/*.elf` scripts at `out/`.
- **Pinned toolchain, single source of truth (#33).** The image bumps
  `arm-none-eabi-gcc` to 15.2 (the 13.2 pin silently could not scan C++20 module
  imports) and `stm32_sdk.cmake` now fails configuration below GCC 14.
  `stmtool doctor` parses the compiler version and flags a stale toolchain, and
  reports whether the SDK image is present.
- **Reusable mock buses (#34).** A new module `testing.mock` ships
  `testing::MockI2c` / `MockSpi` / `MockUart` / `MockGpioPin` / `MockFlash` —
  plain structs that satisfy the driver concepts, programmable with scripted
  responses and write capture, so driver / sensor logic is unit-testable with no
  hardware.
- **Host unit tests + `stmtool test` (#35).** `tests/host/` builds the CMSIS-free
  portable layer with the image's host `g++` and runs `ctest` — `Result<T>` /
  `DRV_TRY` invariants, the mocks, and an MPU6050-on-mock example. `stmtool test`
  runs the suite in Docker; a `host-tests` CI job runs the same command.
- **FreeRTOS baked into the image (#47).** The kernel (`V11.1.0`) is cloned once
  at image-build time and picked up via `FETCHCONTENT_SOURCE_DIR_FREERTOS_KERNEL`,
  so FreeRTOS templates no longer re-clone it per project and build offline.
- **CI on the image.** `build.yml` drops the ARM toolchain action, warms a shared
  buildx cache and loads the image from it (hermetic — no dependency on the
  published tag), and a new `docker-image.yml` publishes the image to GHCR.

Notes:

- No SDK C++ API changes; the break is in the build workflow (`--native`, `out/`,
  Docker requirement). See the [upgrade notes](migration.md#new-in-v0113).
- Host tests need GCC 15 (g++-14 miscompiles modules that include `<cstddef>` in
  the global module fragment); the image provides it via the toolchain PPA.

### v0.1.12

Focus: a single, enforced code style. One clang-format configuration applied
across the whole tree, plus mandatory pre-commit hooks so the style and the
commit history cannot drift again (issues #59, #60).

Highlights:

- **Unified clang-format style, Google 2-space / 80-column.** `.clang-format`
  moved to the Google defaults it was closest to: `IndentWidth` / `TabWidth` 2,
  `ColumnLimit` 80, `AccessModifierOffset` -2, and — the actual bug behind #59 —
  `Cpp11BracedListStyle: true`. The previous explicit `Cpp11BracedListStyle:
  false` contradicted the code, which was written `{nullptr}` (no inner spaces),
  and drifted against clang-format 19; braced-init lists now format the Google
  way. The whole SDK and all ten templates were reformatted in one mechanical
  sweep — no behavioural changes.
- **Vendor sources kept out of the formatter.** A new `.clang-format-ignore`
  excludes the third-party CMSIS core / device headers, the newlib / Cortex-M
  runtime glue and the generated font table, so they stay pristine and diffable
  against upstream. clang-format's `*` matches within a single path segment, so
  the patterns spell the depth out explicitly.
- **Mandatory pre-commit hooks.** A new `.pre-commit-config.yaml` wires three
  gates: `clang-format` pinned to 19.1.3 (so the hook, CI and the tree agree
  byte-for-byte) over C / C++ / `.cppm` sources, `conventional-pre-commit` on the
  commit message, and a pre-push `poe ci` run scoped to `tools/stmtool`. Install
  once with `pip install pre-commit && pre-commit install --hook-type pre-commit
  --hook-type commit-msg`.
- **CI format gate.** A new `Format` workflow runs the same pinned clang-format
  hook on every push and PR to `develop`, so a drifting style fails CI instead
  of landing.
- **Contributor guide.** A new `CONTRIBUTING.md` documents the workflow: install
  the hooks, English Conventional Commits, Russian PR descriptions, `poe ci` for
  `stmtool`.

Notes:

- No source-behaviour or API changes — this release is formatting and tooling
  only. Existing downstream projects need no code edits; re-running
  `clang-format -i` with the new config reflows to 2-space / 80-column.
- `.clang-format-ignore` needs clang-format 18+; the pinned hook uses 19.1.3.

### v0.1.11

Focus: quality and type safety — `consteval` validators for the I2C / SPI / UART
configs, thread-safety annotations, and on-device unit-test helpers
(issues #56–#58).

Highlights:

- **`consteval` config validators `i2c()` / `spi()` / `uart()`.** Previously only
  `GpioConfig` / `ExtiConfig` had compile-time validation. The I2C / SPI / UART
  configs are now free structs in the interface modules — `driver::I2cConfig`,
  `driver::SpiConfig`, `driver::UartConfig` — moved out of the implementation
  types (was `I2c::Config`), each next to a free `consteval` validator
  (`driver::i2c()` / `driver::spi()` / `driver::uart()`) in the same style as
  `gpio()` / `exti()`. SPI / UART fields became strong enums with a `None`
  sentinel: `SpiMode` (`Mode0`..`Mode3`), `SpiDataSize` (`Bits8` / `Bits16`),
  `DataBits` (`Eight` / `Nine`), `StopBits` (`One` / `Two`). An invalid config no
  longer compiles. The UART `Config`, previously duplicated between the primary
  template and the DMA specialisation, is now single-sourced.
- **Thread-safety annotations.** A new pure-preprocessor header
  `util/thread_safety.hpp` (in `sdk/core/include/util/`) wraps the Clang
  thread-safety attributes: `CAPABILITY`, `SCOPED_CAPABILITY`, `GUARDED_BY`,
  `REQUIRES`, `ACQUIRE`, `RELEASE`, `EXCLUDES`, `NO_THREAD_SAFETY_ANALYSIS` and
  more. Under Clang (`-Wthread-safety`) they drive static analysis; under GCC
  (which builds the SDK) they are no-ops. Annotated: `rtos::Mutex` (`CAPABILITY`)
  and `rtos::LockGuard` (`SCOPED_CAPABILITY`) in `rtos.hpp`; the PRIMASK critical
  section modelled as a named capability (`system::detail::g_criticalSection`),
  `enterCritical` / `leaveCritical` as `ACQUIRE` / `RELEASE`, and `WorkQueue::_head`
  / `Channel::_ring` as `GUARDED_BY`. This is groundwork — the actual Clang
  analysis pass arrives with clang-tidy in CI (#73); for now everything compiles
  cleanly under GCC `-Werror` and codegen is unchanged (the macros expand to
  nothing).
- **On-device unit-test helpers.** A new header-only `testing/unit_test.hpp` (in
  `sdk/testing/include/testing/`, INTERFACE target `stm32_testing`, flag
  `STM32_USE_TESTING`). GTest-like `ASSERT_*` / `EXPECT_*` with no exceptions, no
  heap, no dynamic linking: `ASSERT_` returns from the test function, `EXPECT_`
  continues. `TEST(name){...}` defines a test as a function; `TestRunner
  runner{writer}` takes an injectable `Writer` (`void(*)(const char*)`);
  `RUN_TEST(runner, name)` runs a test; `runner.summary()` prints "N passed, M
  failed" and returns a bool. Integer formatting is done by hand (no `snprintf`),
  so the same test code compiles and runs both on the host (writer → stdout) and
  on the device (writer → UART). A new `bare-metal/unit-test-demo` template shows
  it off (tests over `driver::Result<T>`). Complements the future host tests
  (#34 / #35).

Notes:

- **Breaking for driver call sites.** The I2C / SPI / UART configs must now be
  wrapped in their validator (`I2c g{*I2C1, i2c({...})}`), SPI / UART fields are
  enums, and the config type moved (`I2c::Config` → `driver::I2cConfig`, needing
  `import driver.i2c;`). See [migration](migration.md#new-in-v0111).
- The thread-safety and testing additions are purely additive — new headers, no
  edits required for existing code.

### v0.1.10

Focus: finishing the concurrency layer — an EXTI driver, a software `Timer`,
component-owned tasks and a ring-mode signal channel (issues #52–#55).

Highlights:

- New **EXTI driver** `driver.stm32f4.exti` (+ concept `driver::IExti`,
  `ExtiConfig` aggregate and `consteval exti({...})` validator). Routes a GPIO
  pin to an EXTI line, configures the edge, and dispatches to a captureless
  callback from `irqHandler()`. CMSIS-only, `reg::*` for all MMIO. Pairs with
  `executor.postFromISR` for the classic defer-out-of-ISR pattern.
- New **`system::Timer`** (`system.timer`, FreeRTOS only): a thin, type-safe
  wrapper over the executor's `postAfter` / `addPeriodic` / `cancel`. Thunk
  callback, no `std::function`. `start` / `startPeriodic` / `stop` / `active`.
- **`Channel<Event, MaxSubs, RingDepth = 1>`** gained a ring mode. The default is
  the v0.1.9 coalescing slot; `RingDepth > 1` delivers every event in FIFO order
  (drop-oldest on overflow, no heap). Ordering is checked by a `consteval`
  self-test on a standalone `detail::EventRing`.
- **Deferred-start `rtos::Task`**: default-constructible plus an idempotent
  `create(...)`, so a component can own its worker task and start it in
  `onStart()` instead of in `main`.
- New `button-events-demo` template: EXTI button → `postFromISR` → debounce
  `Timer` → ring `Channel` → LED, plus a `Heartbeat` component that owns its own
  deferred-start task.

Notes:

- **Additive** — the EXTI driver builds with `STM32_USE_DRIVERS`; `Timer` follows
  the executor under `STM32_USE_FREERTOS`. The `Channel` change is
  source-compatible (new trailing template parameter defaults to the old
  behaviour) and the new `rtos::Task` members don't touch existing call sites.
  See [migration](migration.md#new-in-v0110).
- EXTI ISR priority must be numerically ≥
  `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 on F4) to call `postFromISR`.

### v0.1.9

Focus: a zero-cost concurrency layer on top of the component framework —
work queue, single-thread executor, type-safe signal bus (issues #30–#32).

Highlights:

- Three new modules in `stm32_system`: `system.work_queue`, `system.executor`,
  `system.signal_bus`. Same zero-vtable / zero-heap style as v0.1.7/v0.1.8 —
  callables are `void(*)(void*)` thunks, not `std::function`.
- `WorkQueue` + `WorkItem`: an **intrusive, client-owned** deferred-work queue
  (no heap). `schedule` / `schedulePriority` / `scheduleAfter` / `cancel`,
  `runDue(now)` for tick-driven dispatch and `runOnce()` for bare-metal
  super-loops. Scheduling is idempotent; ordering is verified by a `consteval`
  self-check.
- `SingleThreadExecutor`: binds a `WorkQueue` to one `rtos::Task` + wake
  semaphore. Handlers run **serially on one task** (no per-handler mutex);
  `post` / `postAfter` / `addPeriodic` / `postFromISR`.
- `Channel<Event, MaxSubs>`: **type-safe** pub/sub — an event is a
  trivially-copyable tag struct, subscribers live in a fixed array (no heap),
  fan-out is dispatched on the executor so subscribers never race. Diverges from
  the reference on purpose: no string publisher names, no `reinterpret_cast`.
- New `signal-bus-demo` template: a `Producer` (own task) publishes and a
  reactive `Consumer` (no task of its own) handles events on the executor,
  subscribing in `onBind()`.

Notes:

- **Additive** — the layer is opt-in. The queue core is RTOS-free (CMSIS PRIMASK
  only) and builds always; the executor and signal bus compile only under
  `STM32_USE_FREERTOS`. All existing templates stay unchanged and green. See
  [migration](migration.md#new-in-v019).
- No `<tuple>`, no heap, no runtime polymorphism. `system.work_queue` depends on
  CMSIS only, keeping the queue core host-testable for a future release.

### v0.1.8

Focus: a zero-cost application framework — component lifecycle, DI convention,
composition root (issues #26–#29).

Highlights:

- New `sdk/system/` layer, enabled by `STM32_USE_SYSTEM` (requires
  `STM32_USE_DRIVERS`), linked as `stm32_system`. Two modules:
  `system.component` and `system.bootstrap`.
- `system::Component` is a **C++20 concept** (not a virtual base): a component
  provides `onRegister/onInit/onBind/onStart` (each returning `driver::Status`)
  plus state/criticality/name accessors — the latter free from the non-virtual
  `system::ComponentBase` mix-in. Zero vtable, in the spirit of the v0.1.7
  concept migration.
- Fixed lifecycle phases `Register → Init → Bind → Start`, run as a barrier by
  `system::bootstrap(components...)`, with **criticality**: a `Critical` failure
  aborts bootstrap; a `Common` failure is counted and skipped while the system
  continues degraded. `bootstrap` returns a `BootReport`
  (`status`, `failedComponent`, `failedPhase`, `degraded`).
- DI convention `Config` + `Environment`: compile-time constants vs.
  reference-passed dependencies; a component that needs a generic bus is a
  constrained template (`template <driver::II2c I2cDriver>`).
- Composition root as a `struct`: the dependency graph is the member list
  (constructed in declaration order); the single `bootstrap(...)` argument list
  is the one source of truth for the component set — no manual registration list.
- The `imu-flash-oled-demo` template is rewritten on the framework
  (`ImuSampler` + `DisplayView` + `FlashLogger`) as the worked example.

Notes:

- **Additive** — the framework is opt-in. Every other template is unchanged and
  stays green; only `imu-flash-oled-demo` moved to components. See
  [migration](migration.md#new-in-v018).
- No `<tuple>`, no heap, no runtime polymorphism — `bootstrap` is a variadic
  fold over the component pack. The `system.*` modules depend only on
  `driver.types`, keeping them host-testable for a future release.

### v0.1.7

Focus: migrate driver and sensor interfaces from virtual base classes to
**C++20 concepts** — no vtable on bus calls (issues #17–#25).

Highlights:

- Driver interfaces `IGpioPin`, `II2c`, `ISpi`, `IUart`, `IFlash` are now
  concepts (`interface/i_*.cppm`); `GpioPin`, `I2c`, `Spi`, `Uart`,
  `InternalFlash` drop inheritance and `override`, each guarded by a
  `static_assert(IXxx<Impl>)` self-check. `NullGpioPin` is now a plain `struct`.
- Sensor interfaces `IImu`, `IDisplay`, `IExternalFlash` are now concepts;
  `Mpu6050`, `Ssd1306`, `W25q32` become class templates parameterised on their
  bus (`template <driver::II2c I2cDriver> class Mpu6050`, …). Bus calls are direct.
  CTAD keeps construction unchanged (`sensor::Mpu6050 g{i2c, {...}}`). Each
  sensor module carries a compile-time `static_assert` self-check against a
  trivial mock bus.
- `W25q32` device constants moved to the non-template `sensor::W25q32Spec`
  (`CAPACITY`, `SECTOR_SIZE`, `PAGE_SIZE`, `JEDEC_W25Q32JV`).
- Every value-returning driver/sensor method is now `[[nodiscard]]` (not only
  the `Status` type): ignoring a `Uart::write` byte count or a flash geometry
  value is a build error. Fire-and-forget UART writes use explicit `(void)`.
- Latent bugs surfaced by templating and fixed: `Mpu6050::setAccelRange` /
  `setGyroRange` ignored a `[[nodiscard]] Status`; the init delay loop used a
  deprecated `volatile` increment; `Ssd1306::sendCommands` used a range-for over
  `std::span` that GCC 15 cannot resolve across a module boundary in a template
  member (switched to an index loop).

Notes:

- Source-breaking only for code that named an interface type directly
  (`II2c&`, `sensor::Mpu6050*`) or the moved `W25q32` constants. All in-tree
  templates use the concrete types and needed only the `W25q32Spec` rename and
  pointer-cast tweaks. See [migration](migration.md#upgrading-to-v017).
- No runtime polymorphism / heterogeneous bus containers (not needed on a
  single target). CMake is unchanged — module file names and module names stay.

### v0.1.6

Focus: process and documentation only — no SDK or API changes.

Highlights:

- The release process now mandates **bilingual** GitHub Release notes: an
  `## English` section copied verbatim from `docs/release.md` and a `## Русский`
  section from `docs/release.ru.md`, separated by `---`, with a footer linking
  the docs release page and the `vPREV...vNEW` changelog. Recorded in `CLAUDE.md`
  (Release process, step 4).

Notes:

- Source-compatible with v0.1.5; downstream projects need no recompilation.

### v0.1.5

Focus: a unified, zero-overhead error-handling layer for `driver.types`
([issue #15](https://github.com/khosta77/stm32-sdk/issues/15),
[issue #16](https://github.com/khosta77/stm32-sdk/issues/16)).

Highlights:

- `driver::Result<T>` — carries either a value of `T` or an error `Status`
  (1-byte tag, no allocation, no exceptions, fully `constexpr`). Marked
  `[[nodiscard]]`; `T` must be trivially destructible. Helpers: `ok()`,
  `status()`, `value()` (precondition `ok()`), `valueOr()`, `operator bool`.
- `DRV_TRY(expr)` and `DRV_TRY_ASSIGN(var, expr)` — Rust-`?`-style early
  return, in the textual header `driver/try.hpp` (macros cannot be exported
  by a C++20 module; include it after `import driver.types;`).
- `driver::Status` is now `[[nodiscard]]`. With the mandatory `-Werror` this
  makes every ignored error a build failure. The in-tree templates were
  updated accordingly: explicit `(void)` on the intentional byte-drop in the
  UART RX ISR, and real error checks around `Mpu6050::init()` and
  `W25q32::eraseSector()` in the FreeRTOS demos.

Notes:

- The `host` unit-test requested in #15/#16 is deferred to v0.1.10, where the
  full host-test harness lands (mock buses #34, host-test CI job #35). For now
  a `consteval` self-check inside `driver.types` validates the `Result<T>`
  invariants at compile time on every ARM build.

### v0.1.4

Focus: quality and infrastructure, no new SDK features beyond
[issue #9](https://github.com/khosta77/stm32-sdk/issues/9).

Highlights:

- `driver::NullGpioPin` — empty `IGpioPin` implementation for boards
  where the SPI CS line is hardwired (issue #9). Drop-in for
  sensors that take `IGpioPin&` such as `W25q32`.
- `stmtool` migrated from `setuptools` to Poetry + `poethepoet`. New
  `poetry run poe ci` runs ruff / flake8 / pylint / black / isort /
  mypy (strict) / bandit / pytest in one shot; `poetry run poe fix`
  applies all auto-fixers. The runtime `__version__` now comes from
  `importlib.metadata`; the generated `_version.py` is gone.
- `stmtool` gained a baseline pytest suite (~80% coverage, 50 tests)
  and a new `.github/workflows/stmtool.yml` CI workflow that runs
  `poe ci` on every PR touching `tools/stmtool/**`.
- `.github/workflows/build.yml` rewritten: the chip matrix is now
  `[STM32F407VG]` only, and each PR builds **all 7 existing templates**
  against it in parallel (`fail-fast: false`). The CI ARM toolchain was
  bumped to `15.2.Rel1`; GCC 13 cannot scan C++20 module imports
  (`-fdeps-format=p1689r5` is GCC 14+), so any template that links
  `stm32_drivers` needs GCC >= 14. The documented minimum is now 14.
- `-Wall -Wextra -Wpedantic -Wshadow -Werror` are now permanently
  enabled on `stm32_core` and propagate to drivers, RTOS, sensors,
  and user code. See [build-flags](build-flags.md) for the policy.
- Project-level `.claude/commands/{ci,fix,build-all-templates,test-template,release-check}.md`
  for contributors using Claude Code.
- New `CLAUDE.md` "Quality enforcement" section: linter rules and
  `-W*` flags must NOT be disabled without explicit user approval.

Known limitations / not validated in CI:

- **STM32F401CE and STM32F411CE** remain supported by the SDK code
  but are no longer covered by CI (the maintainer has no physical
  hardware for these). Use at your own risk; if you build for them
  and hit a warning that breaks `-Werror`, please open an issue.
- `bare-metal/blink/main.cpp` was updated to use `g_ticks = g_ticks + 1`
  instead of `++g_ticks` (the C++20 deprecation of `operator++` on
  volatile). Downstream projects that copied the original may need
  the same one-line fix.

Deferred to v0.1.5+:

- All P1 `stmtool` commands from
  [issue #2](https://github.com/khosta77/stm32-sdk/issues/2):
  `monitor`, `size`, `config`, `project info`, `device list`, `chips`,
  `boards`.
- New bare-metal templates: `uart-echo`, `spi-sensor`, `adc-dma`.
- Board definitions (`.toml` files) and Python chipdb.
- `-Wconversion` warning flag (high noise on CMSIS-derived code,
  needs a separate cleanup pass).

### v0.1.3

Highlights:

- New documentation site (MkDocs Material), English and Russian.
- New `stmtool sdk update`, `sdk list-versions`, `sdk path` subcommands.
- Optional `CLAUDE.md` generation per-template via `--with-claude`.
- All new sensors (MPU6050, SSD1306, W25Q32) and DMA stream wrapper documented.
- I2C multi-byte read fix per RM0090 §27.3.3, valid at 400 kHz.
- `GpioConfig` rewritten as an aggregate with `consteval gpio({...})` validator.

See [upgrade notes](migration.md) for source-side changes required.

### v0.1.2

- `stmtool` install hardening (`install.sh` clears cache on reinstall).

### v0.1.1

- FreeRTOS templates (`freertos/blink`, `freertos/mpu6050-uart`) registered for
  `stmtool` discovery.

### v0.1.0

- Initial release. `setuptools-scm` versioning for `stmtool` and the SDK as a
  whole. CI moved from `main` to `develop`.

## GitHub Pages

The documentation site is published from the `gh-pages` branch (created by the
`docs.yml` workflow). The site is available at
<https://khosta77.github.io/stm32-sdk/>. After the first deployment, enable
Pages in repository Settings → Pages → Source = `gh-pages` / `(root)`.
