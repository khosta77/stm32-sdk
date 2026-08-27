#ifndef STM32_HAL_CMSIS_DEVICE_H_
#define STM32_HAL_CMSIS_DEVICE_H_

// Family-neutral entry point to the CMSIS device header (#98).
//
// Code that is not family-specific -- system.work_queue reaching for PRIMASK,
// a log backend reaching for a core peripheral -- must not spell
// "cmsis/stm32f4xx.h" itself. Every HAL ships this file under its own
// include/ directory, and ${STM32_HAL_DIR}/include is on the include path of
// stm32_drivers / stm32_system / stm32_core, so `#include "cmsis_device.h"`
// resolves to the HAL of whichever family is being built.
//
// The vendor headers under include/cmsis/ stay pristine and are excluded from
// clang-format; this wrapper is ours and is formatted like the rest of the SDK.

#include "cmsis/stm32f4xx.h"

#endif  // STM32_HAL_CMSIS_DEVICE_H_
