#include "cmsis/stm32f4xx.h"
#include "rtos/rtos.hpp"

import driver.types;
import driver.gpio;
import driver.uart;
import driver.reg;
import driver.stm32f4.gpio;
import driver.stm32f4.uart;
import system.component;
import system.bootstrap;
import system.work_queue;
import system.executor;
import system.signal_bus;

extern "C" {
int snprintf(char *str, size_t size, const char *format, ...);
}

using driver::gpio;
using driver::OutputSpeed;
using driver::OutputType;
using driver::Parity;
using driver::PinMode;
using driver::PullMode;
using driver::stm32f4::GpioPin;
using driver::stm32f4::Uart;

extern "C" void __initialize_hardware() {
    SystemCoreClockUpdate();
    driver::reg::set(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIODEN);
    driver::reg::set(RCC->APB1ENR, RCC_APB1ENR_USART2EN);
    __DSB();
}

namespace {

struct HeartbeatEvent {
    uint32_t seq;
    int16_t value;
};

using HeartbeatChannel = system::Channel<HeartbeatEvent, 4>;

class Producer : public system::ComponentBase {
public:
    struct Config {
        system::ComponentConfig base;
        uint32_t periodMs;
    };
    struct Environment {
        HeartbeatChannel &bus;
    };

    Producer(const Config &cfg, const Environment &env)
        : system::ComponentBase(cfg.base), _periodMs(cfg.periodMs), _bus(env.bus) {}

    driver::Status onRegister() { return driver::Status::Ok; }
    driver::Status onInit() { return driver::Status::Ok; }
    driver::Status onBind() { return driver::Status::Ok; }
    driver::Status onStart() { return driver::Status::Ok; }

    void run() {
        while (true) {
            const auto value = static_cast<int16_t>(static_cast<int32_t>(_seq % 200) - 100);
            (void) _bus.publish({.seq = _seq, .value = value});
            ++_seq;
            rtos::Task::delay(pdMS_TO_TICKS(_periodMs));
        }
    }

private:
    uint32_t _periodMs;
    HeartbeatChannel &_bus;
    uint32_t _seq{0};
};

template <driver::IUart UartDriver, driver::IGpioPin LedPin>
class Consumer : public system::ComponentBase {
public:
    struct Config {
        system::ComponentConfig base;
    };
    struct Environment {
        HeartbeatChannel &bus;
        UartDriver &uart;
        LedPin &led;
    };

    Consumer(const Config &cfg, const Environment &env)
        : system::ComponentBase(cfg.base), _bus(env.bus), _uart(env.uart), _led(env.led) {}

    driver::Status onRegister() { return driver::Status::Ok; }
    driver::Status onInit() { return driver::Status::Ok; }
    driver::Status onBind() { return _bus.subscribe<&Consumer::onBeat>(*this); }
    driver::Status onStart() { return driver::Status::Ok; }

    void onBeat(const HeartbeatEvent &event) {
        _led.toggle();
        char buf[48];
        const int len = snprintf(
            buf, sizeof(buf), "beat #%lu value=%+d\r\n",
            static_cast<unsigned long>(event.seq), static_cast<int>(event.value));
        if (len > 0) {
            (void) _uart.write(
                {reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)});
        }
    }

private:
    HeartbeatChannel &_bus;
    UartDriver &_uart;
    LedPin &_led;
};

struct SignalDemo {
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
    GpioPin led{
        *GPIOD,
        gpio({
            .pin = 12,
            .mode = PinMode::Output,
            .pull = PullMode::None,
            .speed = OutputSpeed::Low,
            .type = OutputType::PushPull,
        }),
    };

    Uart<> uart2{
        *USART2,
        USART2_IRQn,
        {
            .baudrate = 115200,
            .dataBits = 8,
            .stopBits = 1,
            .parity = Parity::None,
        },
    };

    system::SingleThreadExecutor exec{
        {
            .name = "sig",
            .stackDepth = 512,
            .priority = 2,
        },
    };
    HeartbeatChannel bus{exec};

    Producer producer{
        {
            .base = {.name = "prod", .criticality = system::Criticality::Common},
            .periodMs = 500,
        },
        {.bus = bus},
    };
    Consumer<Uart<>, GpioPin> consumer{
        {
            .base = {.name = "cons", .criticality = system::Criticality::Common},
        },
        {.bus = bus, .uart = uart2, .led = led},
    };

    [[nodiscard]] system::BootReport boot() { return system::bootstrap(producer, consumer); }
};

static_assert(system::Component<Producer>);
static_assert(system::Component<Consumer<Uart<>, GpioPin>>);

SignalDemo app;

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
        (void) app.uart2.write({reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len)});
    }
}

uint32_t g_selfCheckOrder = 0;

struct SelfCheckSink {
    uint32_t tag;
    void hit() { g_selfCheckOrder = (g_selfCheckOrder << 4) | tag; }
};

bool workQueueDispatchSelfCheck() {
    SelfCheckSink s1{1};
    SelfCheckSink s2{2};
    SelfCheckSink s3{3};
    system::WorkItem w1 = system::WorkItem::bind<&SelfCheckSink::hit>(s1);
    system::WorkItem w2 = system::WorkItem::bind<&SelfCheckSink::hit>(s2);
    system::WorkItem w3 = system::WorkItem::bind<&SelfCheckSink::hit>(s3);
    system::WorkQueue q;
    g_selfCheckOrder = 0;
    q.schedule(w1);
    q.schedule(w2);
    q.schedulePriority(w3);
    const size_t ran = q.runOnce();
    return ran == 3 && g_selfCheckOrder == 0x312U && !q.pending();
}

void producerEntry(void *p) { static_cast<decltype(&app.producer)>(p)->run(); }

}  // namespace

extern "C" void USART2_IRQHandler() {
    app.uart2.irqHandler();
}

int main() {
    const system::BootReport report = app.boot();

    writeStr("\r\n=== signal-bus-demo bootstrap ===\r\n");
    if (report.status != driver::Status::Ok) {
        writePrintf(
            "CRITICAL: %s failed (status=%d, phase=%d)\r\n",
            report.failedComponent,
            static_cast<int>(report.status),
            static_cast<int>(report.failedPhase));
    } else if (report.degraded > 0) {
        writePrintf("degraded: %u component(s) failed, continuing\r\n", report.degraded);
    } else {
        writeStr("all components started\r\n");
    }

    writeStr(workQueueDispatchSelfCheck() ? "workqueue self-check: PASS\r\n"
                                          : "workqueue self-check: FAIL\r\n");

    static rtos::Task tProducer("prod", 384, 2, producerEntry, &app.producer);

    rtos::Task::startScheduler();

    while (true) {
    }
}
