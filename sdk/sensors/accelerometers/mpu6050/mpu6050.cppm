module;
#include <cstddef>
#include <cstdint>
#include <span>
export module sensor.mpu6050;

import driver.i2c;
import driver.types;
import sensor.imu;

export namespace sensor {

template <driver::II2c I2cDriver>
class Mpu6050 {
public:
    struct Config {
        uint8_t addr;
        uint8_t accelRange;
        uint16_t gyroRange;
        uint8_t sampleRateDiv;
        uint8_t dlpfMode;
    };

private:
    struct Regs {
        static constexpr uint8_t SMPLRT_DIV = 0x19;
        static constexpr uint8_t CONFIG = 0x1A;
        static constexpr uint8_t GYRO_CONFIG = 0x1B;
        static constexpr uint8_t ACCEL_CONFIG = 0x1C;
        static constexpr uint8_t ACCEL_XOUT_H = 0x3B;
        static constexpr uint8_t GYRO_XOUT_H = 0x43;
        static constexpr uint8_t PWR_MGMT_1 = 0x6B;
        static constexpr uint8_t WHO_AM_I = 0x75;
    };

    I2cDriver &_i2c;
    Config _cfg;
    float _accelScale = 16384.0f;
    float _gyroScale = 131.0f;

    driver::Status writeReg(uint8_t reg, uint8_t val) {
        return _i2c.writeReg(_cfg.addr, reg, std::span<const uint8_t>(&val, 1));
    }

    driver::Status readRegs(uint8_t reg, uint8_t *buf, size_t len) {
        return _i2c.readReg(_cfg.addr, reg, std::span<uint8_t>(buf, len));
    }

    static int16_t combine(uint8_t hi, uint8_t lo) {
        return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
    }

public:
    explicit Mpu6050(I2cDriver &i2c, const Config &cfg) : _i2c(i2c), _cfg(cfg) {}

    [[nodiscard]] driver::Status init() {
        driver::Status st;

        st = writeReg(Regs::PWR_MGMT_1, 0x80);
        if (st != driver::Status::Ok) {
            return st;
        }

        // MPU-6050 datasheet (rev 4.2, reg 0x6B PWR_MGMT_1, bit 7 DEVICE_RESET):
        // wait >=100 ms after DEVICE_RESET before issuing the next register write.
        // Previous value of 100000 iterations yielded ~0.6 ms at 168 MHz HCLK,
        // i.e. ~150x below spec, so the chip silently stayed in default sleep
        // state on some boards: I2C slave kept ACKing at 0x68, but ACCEL/GYRO
        // data registers always returned 0x0000 (|a| == 0 even at rest, where
        // gravity must give ~9.81 m/s^2). The volatile asm nop in the body
        // prevents the loop from being elided under -O2/-O3.
        // First reproduced and worked around in mpu-6050-logger/src/main.cpp
        // (mpuInitRobust() bypassed this init entirely with a DWT-based delay).
        for (uint32_t i = 0; i < 16000000U; ++i) {
            __asm volatile("nop");
        }

        st = writeReg(Regs::PWR_MGMT_1, 0x01);
        if (st != driver::Status::Ok) {
            return st;
        }

        st = writeReg(Regs::SMPLRT_DIV, _cfg.sampleRateDiv);
        if (st != driver::Status::Ok) {
            return st;
        }

        st = writeReg(Regs::CONFIG, _cfg.dlpfMode);
        if (st != driver::Status::Ok) {
            return st;
        }

        setAccelRange(_cfg.accelRange);
        setGyroRange(_cfg.gyroRange);

        uint8_t whoami = 0;
        st = readRegs(Regs::WHO_AM_I, &whoami, 1);
        if (st != driver::Status::Ok) {
            return st;
        }

        return (whoami == 0x68) ? driver::Status::Ok : driver::Status::HardwareError;
    }

    [[nodiscard]] driver::Status read(ImuData &out) {
        uint8_t buf[14];
        driver::Status st = readRegs(Regs::ACCEL_XOUT_H, buf, 14);
        if (st != driver::Status::Ok) {
            return st;
        }

        out.accel.x = static_cast<float>(combine(buf[0], buf[1])) / _accelScale * 9.80665f;
        out.accel.y = static_cast<float>(combine(buf[2], buf[3])) / _accelScale * 9.80665f;
        out.accel.z = static_cast<float>(combine(buf[4], buf[5])) / _accelScale * 9.80665f;

        int16_t rawTemp = combine(buf[6], buf[7]);
        out.temp = static_cast<float>(rawTemp) / 340.0f + 36.53f;

        out.gyro.x = static_cast<float>(combine(buf[8], buf[9])) / _gyroScale;
        out.gyro.y = static_cast<float>(combine(buf[10], buf[11])) / _gyroScale;
        out.gyro.z = static_cast<float>(combine(buf[12], buf[13])) / _gyroScale;

        return driver::Status::Ok;
    }

    [[nodiscard]] driver::Status selfTest() {
        uint8_t whoami = 0;
        driver::Status st = readRegs(Regs::WHO_AM_I, &whoami, 1);
        if (st != driver::Status::Ok) {
            return st;
        }
        return (whoami == 0x68) ? driver::Status::Ok : driver::Status::HardwareError;
    }

    void setAccelRange(uint8_t g) {
        uint8_t val;
        switch (g) {
            case 4:
                val = 0x08;
                _accelScale = 8192.0f;
                break;
            case 8:
                val = 0x10;
                _accelScale = 4096.0f;
                break;
            case 16:
                val = 0x18;
                _accelScale = 2048.0f;
                break;
            default:
                val = 0x00;
                _accelScale = 16384.0f;
                break;
        }
        (void) writeReg(Regs::ACCEL_CONFIG, val);
    }

    void setGyroRange(uint16_t dps) {
        uint8_t val;
        switch (dps) {
            case 500:
                val = 0x08;
                _gyroScale = 65.5f;
                break;
            case 1000:
                val = 0x10;
                _gyroScale = 32.8f;
                break;
            case 2000:
                val = 0x18;
                _gyroScale = 16.4f;
                break;
            default:
                val = 0x00;
                _gyroScale = 131.0f;
                break;
        }
        (void) writeReg(Regs::GYRO_CONFIG, val);
    }
};

}  // namespace sensor

namespace sensor::detail {

// Zero-infra compile-time check: a trivial bus models driver::II2c, and
// Mpu6050 instantiated on it models sensor::IImu. Catches signature drift on
// every ARM build without hardware. Not exported.
struct MockI2c {
    driver::Status write(uint8_t, std::span<const uint8_t>) { return driver::Status::Ok; }
    driver::Status read(uint8_t, std::span<uint8_t>) { return driver::Status::Ok; }
    driver::Status writeReg(uint8_t, uint8_t, std::span<const uint8_t>) { return driver::Status::Ok; }
    driver::Status readReg(uint8_t, uint8_t, std::span<uint8_t>) { return driver::Status::Ok; }
    driver::Status probe(uint8_t) { return driver::Status::Ok; }
};
static_assert(IImu<Mpu6050<MockI2c>>, "Mpu6050 must model sensor::IImu");

}  // namespace sensor::detail
