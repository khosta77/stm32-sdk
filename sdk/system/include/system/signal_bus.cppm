module;
#include <cstddef>
#include <cstdint>
#include <type_traits>
export module system.signal_bus;

import driver.types;
import system.work_queue;
import system.executor;

namespace system::detail {

// Fixed-capacity event ring with a drop-oldest overflow policy. At Depth == 1 it
// degenerates into the single coalescing slot ("last value wins"); at Depth > 1
// it preserves FIFO order and drops the oldest event on overflow. Kept free of
// the executor so its ordering can be checked in a consteval self-test.
template <class Event, size_t Depth>
struct EventRing {
    static_assert(Depth > 0, "EventRing needs at least one slot");

    Event slots[Depth]{};
    size_t head{0};
    size_t tail{0};
    size_t count{0};

    constexpr void push(const Event &event) {
        slots[head] = event;
        head = (head + 1) % Depth;
        if (count == Depth) {
            tail = (tail + 1) % Depth;
        } else {
            ++count;
        }
    }

    constexpr bool pop(Event &out) {
        if (count == 0) {
            return false;
        }
        out = slots[tail];
        tail = (tail + 1) % Depth;
        --count;
        return true;
    }

    [[nodiscard]] constexpr size_t size() const { return count; }
};

consteval bool eventRingSelfCheck() {
    EventRing<int, 1> single;
    single.push(1);
    single.push(2);
    single.push(3);  // coalesces: only the latest survives
    int value = 0;
    if (!single.pop(value) || value != 3 || single.pop(value)) {
        return false;
    }

    EventRing<int, 3> ring;
    ring.push(1);
    ring.push(2);
    ring.push(3);
    ring.push(4);  // overflow: drop-oldest kills 1
    if (!ring.pop(value) || value != 2) {
        return false;
    }
    if (!ring.pop(value) || value != 3) {
        return false;
    }
    if (!ring.pop(value) || value != 4) {
        return false;
    }
    return !ring.pop(value);
}

static_assert(eventRingSelfCheck(), "Channel event ring FIFO/coalesce/drop-oldest broken");

}  // namespace system::detail

export namespace system {

template <class Event, size_t MaxSubs, size_t RingDepth = 1>
class Channel {
    static_assert(std::is_trivially_copyable_v<Event>,
                  "Channel<Event>: Event must be a trivially-copyable tag struct");
    static_assert(MaxSubs > 0, "Channel needs at least one subscriber slot");
    static_assert(RingDepth > 0, "Channel needs at least one event slot");

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
        _ring.push(event);
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
        for (;;) {
            const uint32_t saved = detail::enterCritical();
            Event event{};
            const bool has = _ring.pop(event);
            detail::leaveCritical(saved);
            if (!has) {
                break;
            }
            for (size_t i = 0; i < _count; ++i) {
                _subs[i].thunk(_subs[i].ctx, event);
            }
        }
    }

    SingleThreadExecutor &_exec;
    WorkItem _item;
    Sub _subs[MaxSubs]{};
    size_t _count{0};
    detail::EventRing<Event, RingDepth> _ring{};
};

}  // namespace system
