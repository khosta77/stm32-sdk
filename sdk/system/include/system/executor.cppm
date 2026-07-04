module;
#include <cstddef>
#include <cstdint>
#include "rtos/rtos.hpp"
export module system.executor;

import system.work_queue;

export namespace system {

class SingleThreadExecutor {
public:
  struct Config {
    const char *name;
    uint16_t stackDepth;
    UBaseType_t priority;
  };

  explicit SingleThreadExecutor(const Config &cfg)
      : _task(cfg.name, cfg.stackDepth, cfg.priority, &trampoline, this) {}

  SingleThreadExecutor(const SingleThreadExecutor &) = delete;
  SingleThreadExecutor &operator=(const SingleThreadExecutor &) = delete;

  void post(WorkItem &item) {
    _wq.scheduleAt(item, xTaskGetTickCount(), false);
    _wake.give();
  }

  void postPriority(WorkItem &item) {
    _wq.scheduleAt(item, xTaskGetTickCount(), true);
    _wake.give();
  }

  void postAfter(WorkItem &item, uint32_t delayMs) {
    _wq.scheduleAfter(item, xTaskGetTickCount(), pdMS_TO_TICKS(delayMs));
    _wake.give();
  }

  void addPeriodic(WorkItem &item, uint32_t periodMs) {
    const uint32_t ticks = pdMS_TO_TICKS(periodMs);
    item.setPeriodTicks(ticks);
    _wq.scheduleAfter(item, xTaskGetTickCount(), ticks);
    _wake.give();
  }

  void postFromISR(WorkItem &item) {
    _wq.scheduleAt(item, xTaskGetTickCountFromISR(), false);
    (void) _wake.giveFromISR();
  }

  void cancel(WorkItem &item) { _wq.cancel(item); }

  [[nodiscard]] WorkQueue &queue() { return _wq; }

private:
  static void trampoline(void *self) {
    static_cast<SingleThreadExecutor *>(self)->runLoop();
  }

  void runLoop() {
    for (;;) {
      (void) _wq.runDue(xTaskGetTickCount());
      uint32_t due = 0;
      TickType_t timeout = portMAX_DELAY;
      if (_wq.nextDue(due)) {
        const int32_t diff = static_cast<int32_t>(due - xTaskGetTickCount());
        timeout = diff > 0 ? static_cast<TickType_t>(diff) : 0;
      }
      (void) _wake.take(timeout);
    }
  }

  WorkQueue _wq;
  rtos::BinarySemaphore _wake;
  rtos::Task _task;
};

}  // namespace system
