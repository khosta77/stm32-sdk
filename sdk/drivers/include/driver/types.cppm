module;
#include <cstddef>
#include <cstdint>
#include <type_traits>
export module driver.types;

export namespace driver {

enum class [[nodiscard]] Status : uint8_t {
    Ok,
    None,
    Timeout,
    Nack,
    BusError,
    Busy,
    InvalidArg,
    HardwareError
};

template <typename T>
class [[nodiscard]] Result {
    static_assert(std::is_trivially_destructible_v<T>,
                  "Result<T> requires a trivially destructible T (freestanding)");

public:
    constexpr Result(T value) : _value(value), _status(Status::Ok) {}

    constexpr Result(Status status)
        : _dummy(), _status(status == Status::Ok ? Status::None : status) {}

    constexpr bool ok() const { return _status == Status::Ok; }
    constexpr Status status() const { return _status; }
    constexpr explicit operator bool() const { return ok(); }

    constexpr T value() const { return _value; }
    constexpr T valueOr(T fallback) const { return ok() ? _value : fallback; }

private:
    struct Empty {};
    union {
        T _value;
        Empty _dummy;
    };
    Status _status;
};

}  // namespace driver

namespace driver::detail {

consteval bool resultSelfCheck() {
    Result<int> good{42};
    if (!good.ok() || good.value() != 42 || good.valueOr(0) != 42) {
        return false;
    }
    Result<int> bad{Status::Timeout};
    if (bad.ok() || bad.status() != Status::Timeout || bad.valueOr(7) != 7) {
        return false;
    }
    Result<int> okAsError{Status::Ok};
    return okAsError.status() == Status::None;
}

static_assert(resultSelfCheck(), "Result<T> invariants broken");

}  // namespace driver::detail
