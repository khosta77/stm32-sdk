#ifndef STM32_SDK_DRIVER_TRY_HPP
#define STM32_SDK_DRIVER_TRY_HPP

// Rust-style early-return helpers for driver::Status / driver::Result<T>.
//
// Macros cannot be exported by a C++20 module, so these live in a textual
// header. Include it AFTER `import driver.types;` in the same translation
// unit; the enclosing function must return driver::Status (or driver::Result,
// since Result(Status) is implicit).

// DRV_TRY(expr): expr must evaluate to driver::Status. If not Ok, return it.
#define DRV_TRY(expr)                            \
    do {                                         \
        const ::driver::Status _drv_st = (expr); \
        if (_drv_st != ::driver::Status::Ok) {   \
            return _drv_st;                      \
        }                                        \
    } while (0)

// DRV_TRY_ASSIGN(var, expr): expr must evaluate to driver::Result<T>. On error
// return the error as driver::Status; on success assign the value to var
// (which must be declared beforehand).
#define DRV_TRY_ASSIGN(var, expr)      \
    do {                               \
        auto _drv_res = (expr);        \
        if (!_drv_res.ok()) {          \
            return _drv_res.status();  \
        }                              \
        (var) = _drv_res.value();      \
    } while (0)

#endif  // STM32_SDK_DRIVER_TRY_HPP
