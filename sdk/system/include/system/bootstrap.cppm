module;
#include <cstdint>
export module system.bootstrap;

import driver.types;
import system.component;

export namespace system {

struct BootReport {
  driver::Status status;
  const char *failedComponent;
  ComponentState failedPhase;
  uint8_t degraded;
};

template <system::Component... Cs>
[[nodiscard]] BootReport bootstrap(Cs &...components) {
  BootReport report{driver::Status::Ok, nullptr, ComponentState::New, 0};

  auto runPhase = [&](auto hook, ComponentState success) -> bool {
    auto one = [&](auto &c) -> bool {
      if (c.state() == ComponentState::Failed) {
        return true;
      }
      const driver::Status st = hook(c);
      if (st == driver::Status::Ok) {
        c.setState(success);
        return true;
      }
      c.setState(ComponentState::Failed);
      if (c.criticality() == Criticality::Critical) {
        report.status = st;
        report.failedComponent = c.name();
        report.failedPhase = success;
        return false;
      }
      ++report.degraded;
      return true;
    };
    return (one(components) && ...);
  };

  if (!runPhase(
          [](auto &c) { return c.onRegister(); },
          ComponentState::Registered
      )) {
    return report;
  }
  if (!runPhase(
          [](auto &c) { return c.onInit(); },
          ComponentState::Initialized
      )) {
    return report;
  }
  if (!runPhase([](auto &c) { return c.onBind(); }, ComponentState::Bound)) {
    return report;
  }
  if (!runPhase([](auto &c) { return c.onStart(); }, ComponentState::Started)) {
    return report;
  }
  return report;
}

}  // namespace system
