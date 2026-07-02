module;
#include <concepts>
#include <cstdint>
export module system.component;

import driver.types;

export namespace system {

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

class ComponentBase {
    ComponentConfig _cfg;
    ComponentState _state{ComponentState::New};

public:
    explicit ComponentBase(const ComponentConfig &cfg) : _cfg(cfg) {}

    [[nodiscard]] const char *name() const { return _cfg.name; }
    [[nodiscard]] Criticality criticality() const { return _cfg.criticality; }
    [[nodiscard]] ComponentState state() const { return _state; }
    void setState(ComponentState s) { _state = s; }
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

}  // namespace system

namespace system::detail {

struct MockComponent : ComponentBase {
    MockComponent() : ComponentBase({.name = "mock", .criticality = Criticality::Common}) {}
    driver::Status onRegister() { return driver::Status::Ok; }
    driver::Status onInit() { return driver::Status::Ok; }
    driver::Status onBind() { return driver::Status::Ok; }
    driver::Status onStart() { return driver::Status::Ok; }
};

static_assert(Component<MockComponent>, "ComponentBase + hooks must model Component");

}  // namespace system::detail
