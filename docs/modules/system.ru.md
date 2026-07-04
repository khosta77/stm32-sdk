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

Компонент **моделирует** концепт — само по себе наследование не требуется. На
практике четыре аксессора вы получаете бесплатно из `ComponentBase` и пишете
только четыре хука жизненного цикла:

```cpp
class ComponentBase {  // невиртуальная примесь, без vtable
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
        _mpu(env.i2c, {/* конфиг сенсора */}) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return _mpu.init(); }
  driver::Status onBind() { return driver::Status::Ok; }
  driver::Status onStart() { return driver::Status::Ok; }
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
  const system::BootReport report = app.boot();  // Register→Init→Bind→Start
  // report.status / report.degraded → лог в UART
  static rtos::Task tImu("imu", 384, 2, imuEntry, &app.imu);
  static rtos::Task tView("view", 512, 1, viewEntry, &app.view);
  rtos::Task::startScheduler();
  while (true) {
  }
}
```

Каждый `*Entry(void*)` — статический трамплин, зовущий `component->run()`;
приводите `void*` обратно через `decltype(&app.imu)` (имя шаблона требует
аргументов). `onStart()` — место, где компонент становится активным; в демо
физический запуск задач живёт в `main`, потому что `rtos::Task` стартует в
конструкторе (отложенного старта пока нет).

Полный рабочий пример — в шаблоне `imu-flash-oled-demo`
(`ImuSampler` + `DisplayView` + `FlashLogger` на общих шинах).

---

# Слой конкурентности (v0.1.9)

Поверх каркаса компонентов библиотека `system` даёт три слоистых примитива,
позволяющих компонентам общаться без прямых вызовов друг друга: `WorkQueue`,
`SingleThreadExecutor` и типобезопасную шину сигналов. Все три держат стиль SDK
— **ноль vtable, ноль кучи, хранилище у клиента** — и переиспользуют тот же
механизм thunk'ов `void(*)(void*)`, что и трамплины задач FreeRTOS.

| Модуль | Зависит от | Строится когда |
|--------|------------|----------------|
| `system.work_queue` | только CMSIS (PRIMASK) | всегда (часть `stm32_system`) |
| `system.executor` | `rtos.hpp` (FreeRTOS) | только с `STM32_USE_FREERTOS` |
| `system.signal_bus` | executor + work queue | только с `STM32_USE_FREERTOS` |

Ядро очереди намеренно RTOS-free: время инъектируется (`runDue(now)`), поэтому
оно же приводит bare-metal super-loop через `runOnce()` и остаётся
host-тестируемым.

## `WorkQueue` и `WorkItem` — отложенная работа без аллокаций

`WorkItem` — **интрузивный узел, которым владеет клиент**: клиент держит его
членом на всё время жизни, поэтому постановка в очередь ничего не аллоцирует.
Узел несёт thunk `void(*)(void*)` плюс контекст; метод привязывается
captureless-фабрикой:

```cpp
system::WorkItem item = system::WorkItem::bind<&MyType::onWork>(myObject);
```

`WorkQueue` — не-шаблонный интрузивный список, упорядоченный по due-тику:

```cpp
void schedule(WorkItem &);          // FIFO, due на эпохе (super-loop)
void schedulePriority(WorkItem &);  // вперёд равных по due
void scheduleAt(WorkItem &, uint32_t due, bool priority);  // абсолютный тик
void scheduleAfter(WorkItem &, uint32_t now, uint32_t delay);
void cancel(WorkItem &);
size_t runDue(uint32_t now);  // исполнить всё с due ≤ now
size_t runOnce();             // слить всё, игнорируя due (super-loop)
bool nextDue(uint32_t &out) const;
bool pending() const;
```

Два свойства несущие:

- **Идемпотентная постановка.** Уже стоящий в очереди `WorkItem` повторно не
  вставляется; второй `schedule()` до исполнения — no-op. Именно это позволяет
  каналу *коалесцировать* повторные публикации, а не рвать список.
- **Обработчики исполняются вне критической секции.** `runDue`/`runOnce`
  отцепляют элемент под короткой CMSIS-PRIMASK-секцией, затем зовут thunk с
  восстановленными прерываниями — поэтому обработчик может пере-планировать себя
  (периодические авто-перевзводятся на `due + period`).

Порядок (FIFO / priority / cancel) проверяется в compile-time `consteval`
self-check'ом внутри модуля — тот же паттерн, что `resultSelfCheck` в
`driver.types`.

### Thread-safety аннотации (v0.1.11)

PRIMASK-критсекция смоделирована как Clang thread-safety **capability**
(`util/thread_safety.hpp`): `system::detail::g_criticalSection` — токен,
`enterCritical()` / `leaveCritical()` размечены `ACQUIRE` / `RELEASE`, а головы
интрузивных списков, которых касаются только внутри секции — `WorkQueue::_head`
и `Channel::_ring` — помечены `GUARDED_BY`. `rtos::Mutex` / `rtos::LockGuard`
несут `CAPABILITY` / `SCOPED_CAPABILITY`. Под Clang `-Wthread-safety` это даёт
статические проверки «поле трогается только под своей блокировкой»; под GCC
(компилятор SDK) каждый макрос разворачивается в пусто, поэтому кодоген не
меняется. Прогон анализа в CI приедет с clang-tidy (issue #73).

## `SingleThreadExecutor` — системный поток

Executor связывает `WorkQueue` с одной выделенной `rtos::Task` и семафором
пробуждения. Обработчики, поставленные ему, исполняются **последовательно на
этой одной задаче**, поэтому гонок между ними нет by design — без мьютекса на
обработчик.

```cpp
system::SingleThreadExecutor exec{{.name = "sys", .stackDepth = 512, .priority = 2}};

exec.post(item);                 // исполнить как можно скорее на задаче executor
exec.postAfter(item, 50);        // исполнить через 50 мс
exec.addPeriodic(item, 1000);    // повторять каждые 1000 мс (перевзвод в ядре)
exec.postFromISR(item);          // отложить работу из ISR
```

Его цикл зовёт `runDue(xTaskGetTickCount())`, затем спит на семафоре до
ближайшего due-элемента (или пока `post` не разбудит). Как всякий объект SDK,
executor — глобальный член composition-root: задача создаётся на static-init, но
бежит только после `vTaskStartScheduler()`.

## Шина сигналов — типобезопасный `Channel<Event, MaxSubs>`

Модули общаются через типизированные каналы, а не строки и `reinterpret_cast`.
Событие — любая тривиально копируемая тег-структура; канал держит фикс-массив из
`MaxSubs` слотов подписчиков (без кучи) и собственный `WorkItem`:

```cpp
struct HeartbeatEvent { uint32_t seq; int16_t value; };
system::Channel<HeartbeatEvent, 4> bus{exec};

// подписать метод-обработчик (возвращает не-Ok, если таблица полна)
driver::Status st = bus.subscribe<&Consumer::onBeat>(consumer);

// опубликовать — веерная рассылка идёт позже, последовательно, на executor
(void) bus.publish({.seq = n, .value = v});
```

`publish` сохраняет событие под критической секцией и ставит `WorkItem` канала в
executor; `dispatch()` затем копирует событие и зовёт каждого подписчика.
Поскольку доставка сериализована на executor, подписчики не гоняются.
Одиночный слот события по умолчанию **коалесцирует** — если две публикации
попадут до dispatch, подписчики увидят последнюю (идеально для семантики
«последняя выборка»). Передайте третий шаблонный аргумент `RingDepth`, если
нужно каждое событие — см. ниже.

Обработчики должны быть короткими — они бегут на общей задаче executor. Тяжёлую
работу откладывайте, поставив из обработчика отдельный `WorkItem` обратно в
executor.

Пример на два компонента — в шаблоне `signal-bus-demo`: `Producer` со своей
задачей публикует, а реактивный `Consumer` **без собственной задачи**
обрабатывает события на executor, подписавшись в `onBind()`.

# Достройка слоя конкурентности (v0.1.10)

Три уточнения поверх v0.1.9, в том же стиле zero-vtable / zero-heap. Шаблон
`button-events-demo` показывает их все вместе.

## `Timer` — отложенные и периодические колбэки

`system::Timer` (модуль `system.timer`, только под FreeRTOS) — тонкая
типобезопасная обёртка над `postAfter` / `addPeriodic` / `cancel` executor'а.
Держит один `WorkItem`; колбэк — captureless-thunk, никогда `std::function`.

```cpp
system::Timer debounce = system::Timer::bind<&Button::onDebounced>(exec, button);

debounce.start(20);           // однократно через 20 мс
debounce.startPeriodic(1000); // повтор каждую секунду
debounce.stop();              // отмена; active() отдаёт признак ожидания
```

`start` сначала снимает прежнюю постановку и обнуляет период, поэтому
перевзвод таймера всегда определён. Обработчики бегут на задаче executor,
сериализованы с остальными — без блокировок внутри колбэка.

## Ring-режим `Channel<Event, MaxSubs, RingDepth>`

У `Channel` появился хвостовой шаблонный параметр `RingDepth` (по умолчанию `1`).
При дефолте поведение ровно как в v0.1.9 — один коалесцирующий слот, последний
выигрывает. При `RingDepth > 1` держится кольцо фиксированной ёмкости и
доставляется **каждое** событие в порядке FIFO, при переполнении вытесняется
самое старое (без кучи).

```cpp
using Bus = system::Channel<ButtonEvent, 4, 8>;  // 4 подписчика, кольцо на 8
```

Кольцо вынесено в самостоятельный `detail::EventRing<Event, Depth>`, поэтому его
порядок (коалесцирование при глубине 1, FIFO + drop-oldest выше) проверяется
`consteval`-самотестом в духе `workQueueOrderSelfCheck`. `dispatch()` дренирует
всё кольцо за одно пробуждение, так что серия публикаций, коалесцирующая в один
`WorkItem`, всё равно доставляет все накопленные события.

## Компоненты со своей задачей (deferred-start)

`rtos::Task` теперь default-конструируем и получил
`create(name, stack, prio, fn, param)` (идемпотентный — повторный вызов no-op).
Это позволяет компоненту владеть своей задачей и стартовать её в `onStart()`, а
не в `main`:

```cpp
driver::Status onStart() {
  return _task.create(_name, _stack, _prio, &trampoline, this)
             ? driver::Status::Ok
             : driver::Status::HardwareError;
}
// ...
rtos::Task _task;  // default-конструируемый член, стартует в onStart()
```

Задачи, созданные до `vTaskStartScheduler()`, спят до запуска планировщика,
поэтому `onStart()` (выполняется до планировщика в `bootstrap`) — естественное
место для их создания. Точка входа остаётся статическим трамплином, приводящим
`void*` обратно к компоненту.
