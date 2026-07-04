# System — component framework

Since v0.1.8 the SDK ships a small, zero-cost application framework under
`sdk/system/include/system/`. It gives an application a **uniform component
lifecycle**, a **dependency-injection convention**, and a **composition root**
pattern — the wiring that in a bare `main.cpp` you would otherwise hand-roll.

The framework is **additive**: existing templates keep working untouched. Only
the `imu-flash-oled-demo` template is written against it, as the worked example.

Like the driver and sensor interfaces (v0.1.7), the contract is a **C++20
concept**, not a virtual base class — there is no vtable, no heap, no runtime
registry. The "registry" is the argument list of a single `bootstrap(...)` call.

Enable it with `STM32_USE_SYSTEM` (requires `STM32_USE_DRIVERS`), link
`stm32_system`. See [build flags](../build-flags.md).

## The `Component` contract

`import system.component;` exports the contract and its helpers
(`export namespace system`).

```cpp
enum class ComponentState : uint8_t {
  New,
  Registered,
  Initialized,
  Bound,
  Started,
  Stopped,
  Failed
};
enum class Criticality : uint8_t {
  Critical,
  Common
};

struct ComponentConfig {
  const char *name;
  Criticality criticality;
};

template <typename T>
concept Component = requires(T c, ComponentState s) {
  { c.onRegister() } -> std::same_as<driver::Status>;
  { c.onInit() } -> std::same_as<driver::Status>;
  { c.onBind() } -> std::same_as<driver::Status>;
  { c.onStart() } -> std::same_as<driver::Status>;
  { c.name() } -> std::same_as<const char *>;
  { c.criticality() } -> std::same_as<Criticality>;
  { c.state() } -> std::same_as<ComponentState>;
  { c.setState(s) } -> std::same_as<void>;
};
```

A component **models** the concept — no inheritance required by the concept
itself. In practice you get the four accessors for free from `ComponentBase`
and only write the four lifecycle hooks:

```cpp
class ComponentBase {  // non-virtual mix-in, no vtable
  ComponentConfig _cfg;
  ComponentState _state{ComponentState::New};

public:
  explicit ComponentBase(const ComponentConfig &cfg) : _cfg(cfg) {}
  [[nodiscard]] const char *name() const;
  [[nodiscard]] Criticality criticality() const;
  [[nodiscard]] ComponentState state() const;
  void setState(ComponentState s);
};
```

The hook names are `onRegister/onInit/onBind/onStart` (not `register` — that is a
C++ keyword). Each returns `driver::Status`. End your component with a
`static_assert(system::Component<MyComponent>)` self-check so a signature drift
is a compile error, exactly like the `static_assert(IXxx<Impl>)` on drivers.

## Lifecycle phases

`import system.bootstrap;` runs the four phases across every component, **as a
barrier**: all `onRegister` first, then all `onInit`, then all `onBind`, then
all `onStart` — mirroring the reference `Register(); Init(); Bind(); Start();`.

| Phase       | Hook          | Success state | Typical work                               |
|-------------|---------------|---------------|--------------------------------------------|
| Register    | `onRegister()`| `Registered`  | announce identity; nothing heavy           |
| Init        | `onInit()`    | `Initialized` | bring up the underlying peripheral/sensor  |
| Bind        | `onBind()`    | `Bound`       | wire cross-component references (v0.1.9 bus)|
| Start       | `onStart()`   | `Started`     | become active                              |

`onBind` is currently a placeholder for the signal bus arriving in v0.1.9; make
it `return driver::Status::Ok;` for now.

## Criticality — degrade instead of crash

Each component declares `Criticality::Critical` or `Criticality::Common`:

- **`Critical`** — the system cannot run correctly without it. A failed hook
  **aborts** bootstrap; the component is left `Failed` and `bootstrap` returns a
  non-`Ok` status.
- **`Common`** — a failure is recorded and counted, the component is left
  `Failed` and **skipped** in later phases, but bootstrap **continues**. The
  system runs degraded rather than not at all.

```cpp
struct BootReport {
  driver::Status status;        // Ok unless a Critical component failed
  const char *failedComponent;  // name of the first Critical failure, else nullptr
  ComponentState failedPhase;   // which phase it failed in
  uint8_t degraded;             // how many Common components degraded
};

template <system::Component... Cs>
[[nodiscard]] BootReport bootstrap(Cs &...components);
```

## DI convention — `Config` + `Environment`

A component takes two constructor structs (the "poor man's DI" — no container,
no runtime):

- **`Config`** — compile-time constants (name, criticality via
  `ComponentConfig base`, periods, thresholds).
- **`Environment`** — references to the things it depends on (buses, other
  components). Swapping a mock into `Environment` is what makes a component
  testable off-target.

```cpp
template <driver::II2c I2cDriver>
class ImuSampler : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
    uint32_t periodMs;
  };
  struct Environment {
    I2cDriver &i2c;
  };

  ImuSampler(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _periodMs(cfg.periodMs),
        _mpu(env.i2c, {/* sensor config */}) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return _mpu.init(); }
  driver::Status onBind() { return driver::Status::Ok; }
  driver::Status onStart() { return driver::Status::Ok; }
  // ...
};
```

A component that needs a generic bus is a **constrained template** — the same
terse `template <driver::II2c I2cDriver>` style as the sensors, with full
descriptive parameter names (`I2cDriver` / `SpiDriver` / `GpioDriver`).

## Composition root — the dependency graph as a `struct`

Declare every driver and component as a member of one `struct`. C++ guarantees
members are constructed **in declaration order**, so dependencies (declared
earlier) are already alive when a later member references them. This is
compile-time wiring — no runtime IoC.

```cpp
struct DemoApp {
  I2c i2c1{*I2C1, {/* ... */}};
  Spi spi2{*SPI2, {/* ... */}};

  ImuSampler<I2c> imu{
      {.base = {.name = "imu", .criticality = system::Criticality::Critical},
       .periodMs = 20},
      {.i2c = i2c1}
  };
  DisplayView<I2c> view{
      {.base = {.name = "oled", .criticality = system::Criticality::Common},
       .periodMs = 200},
      {.i2c = i2c1, .imu = imu}
  };

  [[nodiscard]] system::BootReport boot() {
    return system::bootstrap(imu, view);
  }
};
```

The single `bootstrap(imu, view)` argument list is the **one source of truth**
for the component set — there is no separate manual registration list to keep in
sync (the pitfall the reference architecture calls out).

Give the member types explicitly (`ImuSampler<I2c>`) — CTAD is only needed at
free call sites, not for `struct` members.

## `main()` and FreeRTOS tasks

FreeRTOS tasks created **before** `vTaskStartScheduler()` do not run until the
scheduler starts, so bootstrap (which runs the four hooks in `main`, pre-scheduler)
and task creation can both happen before the scheduler:

```cpp
int main() {
  const system::BootReport report = app.boot();  // Register→Init→Bind→Start
  // report.status / report.degraded → log over UART
  static rtos::Task tImu("imu", 384, 2, imuEntry, &app.imu);
  static rtos::Task tView("view", 512, 1, viewEntry, &app.view);
  rtos::Task::startScheduler();
  while (true) {
  }
}
```

Each `*Entry(void*)` is a static trampoline calling `component->run()`; cast the
`void*` back with `decltype(&app.imu)` (a template name needs its arguments).
`onStart()` is where a component becomes active — in the demo the physical task
launch lives in `main` because `rtos::Task` starts in its constructor (no
deferred start yet).

See the `imu-flash-oled-demo` template for the full worked example
(`ImuSampler` + `DisplayView` + `FlashLogger` on shared buses).

---

# Concurrency layer (v0.1.9)

On top of the component framework the `system` library ships three layered
primitives that let components talk without calling each other directly:
`WorkQueue`, `SingleThreadExecutor`, and a type-safe signal bus. All three keep
the SDK style — **zero vtable, zero heap, client-owned storage** — and reuse the
same `void(*)(void*)` thunk mechanism as the FreeRTOS task trampolines.

| Module | Depends on | Built when |
|--------|------------|------------|
| `system.work_queue` | CMSIS only (PRIMASK) | always (part of `stm32_system`) |
| `system.executor` | `rtos.hpp` (FreeRTOS) | only with `STM32_USE_FREERTOS` |
| `system.signal_bus` | executor + work queue | only with `STM32_USE_FREERTOS` |

The queue core is RTOS-free on purpose: time is injected (`runDue(now)`), so it
also drives bare-metal super-loops via `runOnce()` and stays host-testable.

## `WorkQueue` and `WorkItem` — zero-alloc deferred work

A `WorkItem` is an **intrusive, client-owned** node: the client keeps it as a
member for its whole lifetime, so scheduling never allocates. It carries a
`void(*)(void*)` thunk plus context; bind a member function with the captureless
factory:

```cpp
system::WorkItem item = system::WorkItem::bind<&MyType::onWork>(myObject);
```

`WorkQueue` is a non-template intrusive list ordered by due-tick:

```cpp
void schedule(WorkItem &);          // FIFO, due at epoch (super-loop)
void schedulePriority(WorkItem &);  // jump ahead of equal-due items
void scheduleAt(WorkItem &, uint32_t due, bool priority);  // absolute tick
void scheduleAfter(WorkItem &, uint32_t now, uint32_t delay);
void cancel(WorkItem &);
size_t runDue(uint32_t now);  // run everything due ≤ now
size_t runOnce();             // drain all, ignoring due-tick (super-loop)
bool nextDue(uint32_t &out) const;
bool pending() const;
```

Two properties are load-bearing:

- **Idempotent scheduling.** A `WorkItem` already in the queue is not re-linked;
  a second `schedule()` before it runs is a no-op. This is what makes a channel
  *coalesce* repeated publishes instead of corrupting the list.
- **Handlers run outside the critical section.** `runDue`/`runOnce` unlink the
  item under a brief CMSIS PRIMASK critical section, then call the thunk with
  interrupts restored — so a handler may re-schedule itself (periodic items
  auto-rearm at `due + period`).

Ordering (FIFO / priority / cancel) is verified at compile time by a
`consteval` self-check inside the module, the same pattern as `resultSelfCheck`
in `driver.types`.

### Thread-safety annotations (v0.1.11)

The PRIMASK critical section is modelled as a Clang thread-safety **capability**
(`util/thread_safety.hpp`): `system::detail::g_criticalSection` is the token,
`enterCritical()` / `leaveCritical()` are annotated `ACQUIRE` / `RELEASE`, and
the intrusive list heads touched only inside a section — `WorkQueue::_head` and
`Channel::_ring` — are `GUARDED_BY` it. `rtos::Mutex` / `rtos::LockGuard` carry
`CAPABILITY` / `SCOPED_CAPABILITY`. Under Clang `-Wthread-safety` these drive
static "field only touched under its lock" checks; under GCC (the SDK's
compiler) every macro expands to nothing, so codegen is unchanged. Wiring the
analysis into CI lands with clang-tidy (issue #73).

## `SingleThreadExecutor` — the system thread

The executor binds a `WorkQueue` to one dedicated `rtos::Task` and a wake
semaphore. Handlers posted to it run **serially on that one task**, so there are
no data races between them by design — no per-handler mutex.

```cpp
system::SingleThreadExecutor exec{{.name = "sys", .stackDepth = 512, .priority = 2}};

exec.post(item);                 // run ASAP on the executor task
exec.postAfter(item, 50);        // run after 50 ms
exec.addPeriodic(item, 1000);    // re-run every 1000 ms (core-driven re-arm)
exec.postFromISR(item);          // defer work out of an ISR
```

Its run loop calls `runDue(xTaskGetTickCount())`, then sleeps on the semaphore
until the next due item (or until a `post` wakes it). Like every SDK object, the
executor is a global/composition-root member: its task is created at static init
but only runs after `vTaskStartScheduler()`.

## Signal bus — type-safe `Channel<Event, MaxSubs>`

Modules communicate through typed channels, not strings and `reinterpret_cast`.
An event is any trivially-copyable tag struct; a channel holds a fixed array of
`MaxSubs` subscriber slots (no heap) and its own `WorkItem`:

```cpp
struct HeartbeatEvent { uint32_t seq; int16_t value; };
system::Channel<HeartbeatEvent, 4> bus{exec};

// subscribe a member handler (returns non-Ok if the table is full)
driver::Status st = bus.subscribe<&Consumer::onBeat>(consumer);

// publish — fan-out runs later, serially, on the executor
(void) bus.publish({.seq = n, .value = v});
```

`publish` stores the event under a critical section and posts the channel's
`WorkItem` to the executor; `dispatch()` then copies the event and calls each
subscriber. Because delivery is serialized on the executor, subscribers never
race. The default single event slot **coalesces** — if two publishes land before
dispatch, subscribers see the latest (ideal for "latest sample" semantics). Pass
a third `RingDepth` template argument if you need every event — see below.

Handlers must be short — they run on the shared executor task. For heavy work,
post a separate `WorkItem` back to the executor from inside the handler.

See the `signal-bus-demo` template for a two-component example: a `Producer`
with its own task publishes, and a reactive `Consumer` with **no task of its
own** handles events on the executor, subscribing in `onBind()`.

# Concurrency layer additions (v0.1.10)

Three refinements on top of v0.1.9, same zero-vtable / zero-heap style. The
`button-events-demo` template exercises all of them together.

## `Timer` — deferred and periodic callbacks

`system::Timer` (module `system.timer`, FreeRTOS only) is a thin, type-safe
wrapper over the executor's `postAfter` / `addPeriodic` / `cancel`. It owns one
`WorkItem`; the callback is a captureless thunk, never `std::function`.

```cpp
system::Timer debounce = system::Timer::bind<&Button::onDebounced>(exec, button);

debounce.start(20);           // one-shot after 20 ms
debounce.startPeriodic(1000); // re-fire every second
debounce.stop();              // cancel; active() reports pending state
```

`start` first cancels any prior arming and clears the period, so re-arming a
timer is always well-defined. Handlers run on the executor task, serialized with
every other posted item — no locking inside the callback.

## Ring-mode `Channel<Event, MaxSubs, RingDepth>`

`Channel` gained a trailing `RingDepth` template parameter (default `1`). At the
default it behaves exactly as in v0.1.9 — a single coalescing slot, latest wins.
At `RingDepth > 1` it keeps a fixed-capacity ring and delivers **every** event in
FIFO order, dropping the oldest on overflow (no heap).

```cpp
using Bus = system::Channel<ButtonEvent, 4, 8>;  // 4 subscribers, 8-deep ring
```

The ring lives in a standalone `detail::EventRing<Event, Depth>` so its ordering
(coalesce at depth 1, FIFO + drop-oldest above) is checked by a `consteval`
self-test, mirroring `workQueueOrderSelfCheck`. `dispatch()` drains the whole
ring on each wake, so a burst of publishes that coalesce into one `WorkItem`
still delivers all buffered events.

## Components that own their task (deferred-start)

`rtos::Task` is now default-constructible and gains
`create(name, stack, prio, fn, param)` (idempotent — a second call is a no-op).
This lets a component own its worker task and start it in `onStart()` instead of
in `main`:

```cpp
driver::Status onStart() {
  return _task.create(_name, _stack, _prio, &trampoline, this)
             ? driver::Status::Ok
             : driver::Status::HardwareError;
}
// ...
rtos::Task _task;  // default-constructed member, started in onStart()
```

Tasks created before `vTaskStartScheduler()` stay dormant until the scheduler
runs, so `onStart()` (which runs pre-scheduler in `bootstrap`) is the natural
home for task creation. The task entry stays a static trampoline that casts
`void*` back to the component.
