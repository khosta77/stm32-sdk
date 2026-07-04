module;
#include <cstddef>
#include <cstdint>
#include "cmsis/stm32f4xx.h"
export module system.work_queue;

export namespace system {
class WorkItem;
class WorkQueue;
}  // namespace system

namespace system::detail {

[[nodiscard]] constexpr bool tickLess(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) < 0;
}

constexpr void insertSorted(system::WorkItem *&head, system::WorkItem &node, bool priority);
constexpr bool unlink(system::WorkItem *&head, system::WorkItem &node);
consteval bool workQueueOrderSelfCheck();

export [[nodiscard]] uint32_t enterCritical() {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

export void leaveCritical(uint32_t primask) {
    __set_PRIMASK(primask);
}

}  // namespace system::detail

export namespace system {

class WorkItem {
public:
    using Thunk = void (*)(void *);

    constexpr WorkItem(Thunk thunk, void *ctx) : _thunk(thunk), _ctx(ctx) {}

    template <auto Method, class T>
    [[nodiscard]] static WorkItem bind(T &obj) {
        return WorkItem(&memberThunk<Method, T>, &obj);
    }

    [[nodiscard]] bool queued() const { return _queued; }
    void setPeriodTicks(uint32_t period) { _periodTicks = period; }

private:
    friend class WorkQueue;
    friend constexpr void detail::insertSorted(WorkItem *&, WorkItem &, bool);
    friend constexpr bool detail::unlink(WorkItem *&, WorkItem &);
    friend consteval bool detail::workQueueOrderSelfCheck();

    template <auto Method, class T>
    static void memberThunk(void *ctx) {
        (static_cast<T *>(ctx)->*Method)();
    }

    Thunk _thunk;
    void *_ctx;
    WorkItem *_next{nullptr};
    uint32_t _dueTick{0};
    uint32_t _periodTicks{0};
    bool _queued{false};
};

}  // namespace system

namespace system::detail {

constexpr void insertSorted(system::WorkItem *&head, system::WorkItem &node, bool priority) {
    system::WorkItem **link = &head;
    while (*link != nullptr) {
        const bool keepWalking = priority ? tickLess((*link)->_dueTick, node._dueTick)
                                          : !tickLess(node._dueTick, (*link)->_dueTick);
        if (!keepWalking) {
            break;
        }
        link = &(*link)->_next;
    }
    node._next = *link;
    *link = &node;
}

constexpr bool unlink(system::WorkItem *&head, system::WorkItem &node) {
    for (system::WorkItem **link = &head; *link != nullptr; link = &(*link)->_next) {
        if (*link == &node) {
            *link = node._next;
            node._next = nullptr;
            return true;
        }
    }
    return false;
}

}  // namespace system::detail

export namespace system {

class WorkQueue {
public:
    WorkQueue() = default;
    WorkQueue(const WorkQueue &) = delete;
    WorkQueue &operator=(const WorkQueue &) = delete;

    void schedule(WorkItem &item) { armAt(item, 0, false); }
    void schedulePriority(WorkItem &item) { armAt(item, 0, true); }
    void scheduleAt(WorkItem &item, uint32_t dueTick, bool priority) { armAt(item, dueTick, priority); }
    void scheduleAfter(WorkItem &item, uint32_t nowTick, uint32_t delayTicks) {
        armAt(item, nowTick + delayTicks, false);
    }

    void cancel(WorkItem &item) {
        const uint32_t saved = detail::enterCritical();
        if (item._queued && detail::unlink(_head, item)) {
            item._queued = false;
        }
        detail::leaveCritical(saved);
    }

    size_t runDue(uint32_t now) {
        size_t ran = 0;
        for (;;) {
            const uint32_t saved = detail::enterCritical();
            WorkItem *item = _head;
            if (item == nullptr || detail::tickLess(now, item->_dueTick)) {
                detail::leaveCritical(saved);
                break;
            }
            _head = item->_next;
            item->_next = nullptr;
            item->_queued = false;
            detail::leaveCritical(saved);

            item->_thunk(item->_ctx);
            ++ran;

            if (item->_periodTicks != 0) {
                uint32_t next = item->_dueTick + item->_periodTicks;
                if (detail::tickLess(next, now)) {
                    next = now + item->_periodTicks;
                }
                armAt(*item, next, false);
            }
        }
        return ran;
    }

    size_t runOnce() {
        size_t ran = 0;
        for (;;) {
            const uint32_t saved = detail::enterCritical();
            WorkItem *item = _head;
            if (item == nullptr) {
                detail::leaveCritical(saved);
                break;
            }
            _head = item->_next;
            item->_next = nullptr;
            item->_queued = false;
            detail::leaveCritical(saved);

            item->_thunk(item->_ctx);
            ++ran;
        }
        return ran;
    }

    [[nodiscard]] bool nextDue(uint32_t &out) const {
        const uint32_t saved = detail::enterCritical();
        const bool has = _head != nullptr;
        if (has) {
            out = _head->_dueTick;
        }
        detail::leaveCritical(saved);
        return has;
    }

    [[nodiscard]] bool pending() const {
        const uint32_t saved = detail::enterCritical();
        const bool has = _head != nullptr;
        detail::leaveCritical(saved);
        return has;
    }

private:
    void armAt(WorkItem &item, uint32_t due, bool priority) {
        const uint32_t saved = detail::enterCritical();
        if (!item._queued) {
            item._dueTick = due;
            item._queued = true;
            detail::insertSorted(_head, item, priority);
        }
        detail::leaveCritical(saved);
    }

    WorkItem *_head{nullptr};
};

}  // namespace system

namespace system::detail {

consteval bool workQueueOrderSelfCheck() {
    system::WorkItem a(nullptr, nullptr);
    system::WorkItem b(nullptr, nullptr);
    system::WorkItem c(nullptr, nullptr);
    system::WorkItem d(nullptr, nullptr);
    system::WorkItem *head = nullptr;

    a._dueTick = 10;
    insertSorted(head, a, false);
    b._dueTick = 10;
    insertSorted(head, b, false);  // FIFO: after a
    c._dueTick = 10;
    insertSorted(head, c, true);  // priority: before a
    if (!(head == &c && head->_next == &a && head->_next->_next == &b)) {
        return false;
    }

    d._dueTick = 5;
    insertSorted(head, d, false);  // earliest to front
    if (head != &d) {
        return false;
    }

    if (!unlink(head, a) || unlink(head, a)) {  // cancel once, not twice
        return false;
    }
    return head == &d && head->_next == &c && head->_next->_next == &b;
}

static_assert(workQueueOrderSelfCheck(), "WorkQueue FIFO/priority/cancel ordering broken");

}  // namespace system::detail
