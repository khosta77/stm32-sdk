#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"

import driver.types;
import driver.gpio;
import driver.uart;
import driver.exti;
import driver.reg;
import driver.stm32f4.gpio;
import driver.stm32f4.uart;
import driver.stm32f4.exti;
import system.component;
import system.bootstrap;
import system.work_queue;
import system.executor;
import system.timer;
import system.signal_bus;

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::DataBits;
using driver::exti;
using driver::ExtiPort;
using driver::ExtiTrigger;
using driver::gpio;
using driver::OutputSpeed;
using driver::OutputType;
using driver::Parity;
using driver::PinMode;
using driver::PullMode;
using driver::StopBits;
using driver::uart;
using driver::stm32f4::ExtiLine;
using driver::stm32f4::GpioPin;
using driver::stm32f4::Uart;

extern "C" void __initialize_hardware() {
  SystemCoreClockUpdate();
  driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIODEN);
  driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_USART2EN);
  __DSB();
}

namespace {

struct ButtonEvent {
  uint32_t seq;
  uint32_t tick;
};

// Ring depth 8: every debounced press is delivered, even a fast burst that
// arrives before the executor drains the channel (the coalescing default would
// keep only the latest).
using ButtonChannel = system::Channel<ButtonEvent, 4, 8>;

// Owns the EXTI line + a debounce Timer. The ISR only defers work
// (postFromISR); the actual edge handling and publishing run on the executor
// task, so no FreeRTOS API is called from interrupt context beyond postFromISR.
template <driver::IGpioPin ButtonPin>
class Button : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
    driver::ExtiConfig line;
    uint32_t debounceMs;
  };
  struct Environment {
    system::SingleThreadExecutor &exec;
    ButtonChannel &bus;
    ButtonPin &pin;
  };

  Button(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _debounceMs(cfg.debounceMs),
        _exec(env.exec),
        _bus(env.bus),
        _pin(env.pin),
        _edge(system::WorkItem::bind<&Button::onEdge>(*this)),
        _debounce(system::Timer::bind<&Button::onDebounced>(env.exec, *this)),
        _exti(ExtiLine::bind<&Button::onIsr>(cfg.line, *this)) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return driver::Status::Ok; }
  driver::Status onBind() { return driver::Status::Ok; }
  driver::Status onStart() { return driver::Status::Ok; }

  void irqHandler() { _exti.irqHandler(); }

private:
  void onIsr() { _exec.postFromISR(_edge); }

  void onEdge() { _debounce.start(_debounceMs); }

  void onDebounced() {
    if (_pin.read() == driver::Status::Ok) {
      (void) _bus.publish({.seq = _seq, .tick = xTaskGetTickCount()});
      ++_seq;
    }
  }

  uint32_t _debounceMs;
  system::SingleThreadExecutor &_exec;
  ButtonChannel &_bus;
  ButtonPin &_pin;
  system::WorkItem _edge;
  system::Timer _debounce;
  uint32_t _seq{0};
  ExtiLine _exti;
};

// Reactive subscriber: no task of its own, wakes on the executor task whenever
// a debounced press lands on the channel.
template <driver::IUart UartDriver, driver::IGpioPin LedPin>
class Reporter : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
  };
  struct Environment {
    ButtonChannel &bus;
    UartDriver &uart;
    LedPin &led;
  };

  Reporter(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _bus(env.bus),
        _uart(env.uart),
        _led(env.led) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return driver::Status::Ok; }
  driver::Status onBind() { return _bus.subscribe<&Reporter::onPress>(*this); }
  driver::Status onStart() { return driver::Status::Ok; }

  void onPress(const ButtonEvent &event) {
    _led.toggle();
    char buf[48];
    const int len = snprintf(
        buf,
        sizeof(buf),
        "press #%lu @tick=%lu\r\n",
        static_cast<unsigned long>(event.seq),
        static_cast<unsigned long>(event.tick)
    );
    if (len > 0) {
      (void) _uart.write(
          {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
      );
    }
  }

private:
  ButtonChannel &_bus;
  UartDriver &_uart;
  LedPin &_led;
};

// Owns its own worker task and starts it in onStart() (deferred-start): the
// task is created inside the component, not in main. Tasks created before
// startScheduler() stay dormant until the scheduler runs.
template <driver::IUart UartDriver, driver::IGpioPin LedPin>
class Heartbeat : public system::ComponentBase {
public:
  struct Config {
    system::ComponentConfig base;
    const char *taskName;
    uint16_t stackDepth;
    UBaseType_t priority;
    uint32_t periodMs;
  };
  struct Environment {
    UartDriver &uart;
    LedPin &led;
  };

  Heartbeat(const Config &cfg, const Environment &env)
      : system::ComponentBase(cfg.base),
        _taskName(cfg.taskName),
        _stackDepth(cfg.stackDepth),
        _priority(cfg.priority),
        _periodMs(cfg.periodMs),
        _uart(env.uart),
        _led(env.led) {}

  driver::Status onRegister() { return driver::Status::Ok; }
  driver::Status onInit() { return driver::Status::Ok; }
  driver::Status onBind() { return driver::Status::Ok; }

  driver::Status onStart() {
    return _task.create(_taskName, _stackDepth, _priority, &trampoline, this)
               ? driver::Status::Ok
               : driver::Status::HardwareError;
  }

  void run() {
    uint32_t beat = 0;
    while (true) {
      _led.toggle();
      char buf[32];
      const int len = snprintf(
          buf,
          sizeof(buf),
          "alive #%lu\r\n",
          static_cast<unsigned long>(beat)
      );
      if (len > 0) {
        (void) _uart.write(
            {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
        );
      }
      ++beat;
      rtos::Task::delay(pdMS_TO_TICKS(_periodMs));
    }
  }

private:
  static void trampoline(void *self) { static_cast<Heartbeat *>(self)->run(); }

  const char *_taskName;
  uint16_t _stackDepth;
  UBaseType_t _priority;
  uint32_t _periodMs;
  UartDriver &_uart;
  LedPin &_led;
  rtos::Task _task;
};

struct ButtonDemo {
  GpioPin uartTx{
      *GPIOA,
      gpio({
          .pin = 2,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 7,
      }),
  };
  GpioPin uartRx{
      *GPIOA,
      gpio({
          .pin = 3,
          .mode = PinMode::AlternateFunction,
          .pull = PullMode::None,
          .speed = OutputSpeed::VeryHigh,
          .type = OutputType::PushPull,
          .af = 7,
      }),
  };
  GpioPin button{
      *GPIOA,
      gpio({
          .pin = 0,
          .mode = PinMode::Input,
          .pull = PullMode::None,
          .speed = OutputSpeed::None,
          .type = OutputType::None,
      }),
  };
  GpioPin pressLed{
      *GPIOD,
      gpio({
          .pin = 12,
          .mode = PinMode::Output,
          .pull = PullMode::None,
          .speed = OutputSpeed::Low,
          .type = OutputType::PushPull,
      }),
  };
  GpioPin heartbeatLed{
      *GPIOD,
      gpio({
          .pin = 13,
          .mode = PinMode::Output,
          .pull = PullMode::None,
          .speed = OutputSpeed::Low,
          .type = OutputType::PushPull,
      }),
  };

  Uart<> uart2{
      *USART2,
      USART2_IRQn,
      uart({
          .baudrate = 115200,
          .dataBits = DataBits::Eight,
          .stopBits = StopBits::One,
          .parity = Parity::None,
      }),
  };

  system::SingleThreadExecutor exec{
      {
          .name = "btn",
          .stackDepth = 512,
          .priority = 2,
      },
  };
  ButtonChannel bus{exec};

  Button<GpioPin> btn{
      {
          .base =
              {.name = "button", .criticality = system::Criticality::Common},
          .line = exti({
              .line = 0,
              .port = ExtiPort::A,
              .trigger = ExtiTrigger::Rising,
              .priority = 6,
          }),
          .debounceMs = 20,
      },
      {.exec = exec, .bus = bus, .pin = button},
  };
  Reporter<Uart<>, GpioPin> reporter{
      {
          .base =
              {.name = "report", .criticality = system::Criticality::Common},
      },
      {.bus = bus, .uart = uart2, .led = pressLed},
  };
  Heartbeat<Uart<>, GpioPin> heartbeat{
      {
          .base = {.name = "hbeat", .criticality = system::Criticality::Common},
          .taskName = "hbeat",
          .stackDepth = 384,
          .priority = 2,
          .periodMs = 1000,
      },
      {.uart = uart2, .led = heartbeatLed},
  };

  [[nodiscard]] system::BootReport boot() {
    return system::bootstrap(btn, reporter, heartbeat);
  }
};

static_assert(system::Component<Button<GpioPin>>);
static_assert(system::Component<Reporter<Uart<>, GpioPin>>);
static_assert(system::Component<Heartbeat<Uart<>, GpioPin>>);

ButtonDemo app;

void writeStr(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') {
    ++n;
  }
  (void) app.uart2.write({reinterpret_cast<const uint8_t *>(s), n});
}

void writePrintf(const char *fmt, auto... args) {
  char buf[96];
  const int len = snprintf(buf, sizeof(buf), fmt, args...);
  if (len > 0) {
    (void) app.uart2.write(
        {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)}
    );
  }
}

}  // namespace

extern "C" void EXTI0_IRQHandler() {
  app.btn.irqHandler();
}

extern "C" void USART2_IRQHandler() {
  app.uart2.irqHandler();
}

int main() {
  const system::BootReport report = app.boot();

  writeStr("\r\n=== button-events-demo bootstrap ===\r\n");
  if (report.status != driver::Status::Ok) {
    writePrintf(
        "CRITICAL: %s failed (status=%d, phase=%d)\r\n",
        report.failedComponent,
        static_cast<int>(report.status),
        static_cast<int>(report.failedPhase)
    );
  } else if (report.degraded > 0) {
    writePrintf(
        "degraded: %u component(s) failed, continuing\r\n",
        report.degraded
    );
  } else {
    writeStr("all components started; press the user button (PA0)\r\n");
  }

  rtos::Task::startScheduler();

  while (true) {
  }
}
