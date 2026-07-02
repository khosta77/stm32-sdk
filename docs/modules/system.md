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
    New, Registered, Initialized, Bound, Started, Stopped, Failed
};
enum class Criticality : uint8_t { Critical, Common };

struct ComponentConfig { const char *name; Criticality criticality; };

template <typename T>
concept Component = requires(T c, ComponentState s) {
    { c.onRegister() } -> std::same_as<driver::Status>;
    { c.onInit() }     -> std::same_as<driver::Status>;
    { c.onBind() }     -> std::same_as<driver::Status>;
    { c.onStart() }    -> std::same_as<driver::Status>;
    { c.name() }        -> std::same_as<const char *>;
    { c.criticality() } -> std::same_as<Criticality>;
    { c.state() }       -> std::same_as<ComponentState>;
    { c.setState(s) }   -> std::same_as<void>;
};
```

A component **models** the concept — no inheritance required by the concept
itself. In practice you get the four accessors for free from `ComponentBase`
and only write the four lifecycle hooks:

```cpp
class ComponentBase {                 // non-virtual mix-in, no vtable
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
    struct Config { system::ComponentConfig base; uint32_t periodMs; };
    struct Environment { I2cDriver &i2c; };

    ImuSampler(const Config &cfg, const Environment &env)
        : system::ComponentBase(cfg.base), _periodMs(cfg.periodMs),
          _mpu(env.i2c, {/* sensor config */}) {}

    driver::Status onRegister() { return driver::Status::Ok; }
    driver::Status onInit()     { return _mpu.init(); }
    driver::Status onBind()     { return driver::Status::Ok; }
    driver::Status onStart()    { return driver::Status::Ok; }
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
    I2c  i2c1{*I2C1, {/* ... */}};
    Spi  spi2{*SPI2, {/* ... */}};

    ImuSampler<I2c> imu{
        {.base = {.name = "imu", .criticality = system::Criticality::Critical}, .periodMs = 20},
        {.i2c = i2c1}};
    DisplayView<I2c> view{
        {.base = {.name = "oled", .criticality = system::Criticality::Common}, .periodMs = 200},
        {.i2c = i2c1, .imu = imu}};

    [[nodiscard]] system::BootReport boot() { return system::bootstrap(imu, view); }
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
    const system::BootReport report = app.boot();   // Register→Init→Bind→Start
    // report.status / report.degraded → log over UART
    static rtos::Task tImu("imu", 384, 2, imuEntry, &app.imu);
    static rtos::Task tView("view", 512, 1, viewEntry, &app.view);
    rtos::Task::startScheduler();
    while (true) {}
}
```

Each `*Entry(void*)` is a static trampoline calling `component->run()`; cast the
`void*` back with `decltype(&app.imu)` (a template name needs its arguments).
`onStart()` is where a component becomes active — in the demo the physical task
launch lives in `main` because `rtos::Task` starts in its constructor (no
deferred start yet).

See the `imu-flash-oled-demo` template for the full worked example
(`ImuSampler` + `DisplayView` + `FlashLogger` on shared buses).
