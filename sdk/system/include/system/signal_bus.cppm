module;
#include <cstddef>
#include <cstdint>
#include <type_traits>
export module system.signal_bus;

import driver.types;
import system.work_queue;
import system.executor;

export namespace system {

template <class Event, size_t MaxSubs>
class Channel {
    static_assert(std::is_trivially_copyable_v<Event>,
                  "Channel<Event>: Event must be a trivially-copyable tag struct");
    static_assert(MaxSubs > 0, "Channel needs at least one subscriber slot");

public:
    using Handler = void (*)(void *, const Event &);

    explicit Channel(SingleThreadExecutor &exec) : _exec(exec), _item(&dispatchThunk, this) {}

    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;

    template <auto Method, class T>
    [[nodiscard]] driver::Status subscribe(T &obj) {
        return subscribe(&subThunk<Method, T>, &obj);
    }

    [[nodiscard]] driver::Status subscribe(Handler fn, void *ctx) {
        if (_count >= MaxSubs) {
            return driver::Status::None;
        }
        _subs[_count].thunk = fn;
        _subs[_count].ctx = ctx;
        ++_count;
        return driver::Status::Ok;
    }

    driver::Status publish(const Event &event) {
        const uint32_t saved = detail::enterCritical();
        _pending = event;
        _hasPending = true;
        detail::leaveCritical(saved);
        _exec.post(_item);
        return driver::Status::Ok;
    }

    [[nodiscard]] size_t subscriberCount() const { return _count; }

private:
    struct Sub {
        Handler thunk;
        void *ctx;
    };

    template <auto Method, class T>
    static void subThunk(void *ctx, const Event &event) {
        (static_cast<T *>(ctx)->*Method)(event);
    }

    static void dispatchThunk(void *self) { static_cast<Channel *>(self)->dispatch(); }

    void dispatch() {
        const uint32_t saved = detail::enterCritical();
        if (!_hasPending) {
            detail::leaveCritical(saved);
            return;
        }
        const Event event = _pending;
        _hasPending = false;
        detail::leaveCritical(saved);

        for (size_t i = 0; i < _count; ++i) {
            _subs[i].thunk(_subs[i].ctx, event);
        }
    }

    SingleThreadExecutor &_exec;
    WorkItem _item;
    Sub _subs[MaxSubs]{};
    size_t _count{0};
    Event _pending{};
    bool _hasPending{false};
};

}  // namespace system
