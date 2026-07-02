# System — каркас компонентов

Начиная с v0.1.8 в SDK есть небольшой zero-cost каркас приложения в
`sdk/system/include/system/`. Он даёт приложению **единый жизненный цикл
компонентов**, **соглашение о внедрении зависимостей** и паттерн **composition
root** — ту развязку, которую в голом `main.cpp` пришлось бы собирать вручную.

Каркас **аддитивен**: существующие шаблоны продолжают работать без изменений.
Только шаблон `imu-flash-oled-demo` написан на нём — как рабочий пример.

Как и интерфейсы драйверов и сенсоров (v0.1.7), контракт — это **C++20-концепт**,
а не виртуальный базовый класс: нет vtable, нет кучи, нет рантайм-реестра.
«Реестр» — это список аргументов одного вызова `bootstrap(...)`.

Включается флагом `STM32_USE_SYSTEM` (требует `STM32_USE_DRIVERS`), линкуется
`stm32_system`. См. [флаги сборки](../build-flags.md).

## Контракт `Component`

`import system.component;` экспортирует контракт и его хелперы
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

Компонент **моделирует** концепт — само по себе наследование не требуется. На
практике четыре аксессора вы получаете бесплатно из `ComponentBase` и пишете
только четыре хука жизненного цикла:

```cpp
class ComponentBase {                 // невиртуальная примесь, без vtable
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

Хуки называются `onRegister/onInit/onBind/onStart` (не `register` — это
ключевое слово C++). Каждый возвращает `driver::Status`. Завершайте компонент
self-check'ом `static_assert(system::Component<MyComponent>)`, чтобы дрейф
сигнатуры был ошибкой компиляции — ровно как `static_assert(IXxx<Impl>)` на
драйверах.

## Фазы жизненного цикла

`import system.bootstrap;` прогоняет четыре фазы по всем компонентам **барьером**:
сначала все `onRegister`, затем все `onInit`, затем все `onBind`, затем все
`onStart` — как в референсе `Register(); Init(); Bind(); Start();`.

| Фаза     | Хук           | Состояние при успехе | Типичная работа                              |
|----------|---------------|----------------------|----------------------------------------------|
| Register | `onRegister()`| `Registered`         | заявить идентичность; ничего тяжёлого         |
| Init     | `onInit()`    | `Initialized`        | поднять периферию/сенсор                       |
| Bind     | `onBind()`    | `Bound`              | связать ссылки между компонентами (шина v0.1.9)|
| Start    | `onStart()`   | `Started`            | стать активным                                 |

`onBind` сейчас — задел под шину сигналов из v0.1.9; пока делайте
`return driver::Status::Ok;`.

## Критичность — деградация вместо падения

Каждый компонент объявляет `Criticality::Critical` или `Criticality::Common`:

- **`Critical`** — без него система не работает корректно. Отказ хука
  **останавливает** bootstrap; компонент остаётся `Failed`, а `bootstrap`
  возвращает не-`Ok` статус.
- **`Common`** — отказ записывается и считается, компонент остаётся `Failed` и
  **пропускается** в последующих фазах, но bootstrap **продолжает**. Система
  работает деградированно, а не отказывает целиком.

```cpp
struct BootReport {
    driver::Status status;        // Ok, если ни один Critical не упал
    const char *failedComponent;  // имя первого Critical-отказа, иначе nullptr
    ComponentState failedPhase;   // на какой фазе он упал
    uint8_t degraded;             // сколько Common-компонентов деградировало
};

template <system::Component... Cs>
[[nodiscard]] BootReport bootstrap(Cs &...components);
```

## Соглашение DI — `Config` + `Environment`

Компонент принимает две конструкторские структуры («бедный DI» — без контейнера,
без рантайма):

- **`Config`** — compile-time константы (имя и критичность через
  `ComponentConfig base`, периоды, пороги).
- **`Environment`** — ссылки на зависимости (шины, другие компоненты). Подстановка
  мока в `Environment` — это и есть то, что делает компонент тестируемым вне
  железа.

```cpp
template <driver::II2c I2cDriver>
class ImuSampler : public system::ComponentBase {
public:
    struct Config { system::ComponentConfig base; uint32_t periodMs; };
    struct Environment { I2cDriver &i2c; };

    ImuSampler(const Config &cfg, const Environment &env)
        : system::ComponentBase(cfg.base), _periodMs(cfg.periodMs),
          _mpu(env.i2c, {/* конфиг сенсора */}) {}

    driver::Status onRegister() { return driver::Status::Ok; }
    driver::Status onInit()     { return _mpu.init(); }
    driver::Status onBind()     { return driver::Status::Ok; }
    driver::Status onStart()    { return driver::Status::Ok; }
    // ...
};
```

Компонент, которому нужна обобщённая шина, — это **ограниченный шаблон**: тот же
краткий стиль `template <driver::II2c I2cDriver>`, что и у сенсоров, с полными
описательными именами параметров (`I2cDriver` / `SpiDriver` / `GpioDriver`).

## Composition root — граф зависимостей как `struct`

Объявите каждый драйвер и компонент членом одного `struct`. C++ гарантирует, что
члены конструируются **в порядке объявления**, поэтому зависимости (объявленные
раньше) уже живы, когда на них ссылается более поздний член. Это compile-time
развязка — никакого рантайм-IoC.

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

Единственный список аргументов `bootstrap(imu, view)` — это **единственный
источник правды** о наборе компонентов: нет отдельного ручного списка
регистрации, который надо держать в синхронизации (ровно та ловушка, о которой
предупреждает референсная архитектура).

Типы членов пишите явно (`ImuSampler<I2c>`) — CTAD нужен только в свободных
call-site'ах, не для членов `struct`.

## `main()` и задачи FreeRTOS

Задачи FreeRTOS, созданные **до** `vTaskStartScheduler()`, не исполняются до
старта планировщика, поэтому и bootstrap (он прогоняет четыре хука в `main` до
планировщика), и создание задач могут пройти до старта:

```cpp
int main() {
    const system::BootReport report = app.boot();   // Register→Init→Bind→Start
    // report.status / report.degraded → лог в UART
    static rtos::Task tImu("imu", 384, 2, imuEntry, &app.imu);
    static rtos::Task tView("view", 512, 1, viewEntry, &app.view);
    rtos::Task::startScheduler();
    while (true) {}
}
```

Каждый `*Entry(void*)` — статический трамплин, зовущий `component->run()`;
приводите `void*` обратно через `decltype(&app.imu)` (имя шаблона требует
аргументов). `onStart()` — место, где компонент становится активным; в демо
физический запуск задач живёт в `main`, потому что `rtos::Task` стартует в
конструкторе (отложенного старта пока нет).

Полный рабочий пример — в шаблоне `imu-flash-oled-demo`
(`ImuSampler` + `DisplayView` + `FlashLogger` на общих шинах).
