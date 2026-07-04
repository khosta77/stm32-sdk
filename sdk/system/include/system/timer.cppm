module;
#include <cstdint>
export module system.timer;

import system.work_queue;
import system.executor;

export namespace system {

// Thin, type-safe wrapper over SingleThreadExecutor's deferred/periodic
// posting. A Timer owns a single WorkItem and delegates scheduling to the
// executor's runLoop; the callback is a captureless thunk (void(*)(void*)),
// never std::function. Handlers run on the executor task, so they are
// serialized with every other posted work item -- no extra locking inside the
// callback.
class Timer {
public:
  using Thunk = void (*)(void *);

  Timer(SingleThreadExecutor &exec, Thunk callback, void *ctx)
      : _exec(exec), _item(callback, ctx) {}

  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;

  template <auto Method, class T>
  [[nodiscard]] static Timer bind(SingleThreadExecutor &exec, T &obj) {
    return Timer(exec, &memberThunk<Method, T>, &obj);
  }

  void start(uint32_t delayMs) {
    _exec.cancel(_item);
    _item.setPeriodTicks(0);
    _exec.postAfter(_item, delayMs);
  }

  void startPeriodic(uint32_t periodMs) {
    _exec.cancel(_item);
    _exec.addPeriodic(_item, periodMs);
  }

  void stop() {
    _exec.cancel(_item);
    _item.setPeriodTicks(0);
  }

  [[nodiscard]] bool active() const { return _item.queued(); }

private:
  template <auto Method, class T>
  static void memberThunk(void *ctx) {
    (static_cast<T *>(ctx)->*Method)();
  }

  SingleThreadExecutor &_exec;
  WorkItem _item;
};

}  // namespace system
