module;
export module driver.null_gpio;

import driver.gpio;
import driver.types;

export namespace driver {

class NullGpioPin : public IGpioPin {
public:
    void set() override {}
    void reset() override {}
    void toggle() override {}
    Status read() override { return Status::None; }
};

}  // namespace driver
