#ifndef STM32_SDK_DRIVER_LOG_HPP
#define STM32_SDK_DRIVER_LOG_HPP

// Compile-time LOG_* macros for the driver.log facility (issues #36, #37).
//
// Macros cannot be exported by a C++20 module, so they live in this textual
// header. Include it AFTER `import driver.log;` in the same translation unit.
//
// STM32_LOG_LEVEL is the compile-time ceiling (default Info): every LOG_* above
// it expands to ((void)0), so both the call and its format string are removed
// from the binary -- zero cost, nothing left in flash. The build sets it via
// -DSTM32_LOG_LEVEL=<0..5> (cmake option STM32_LOG_LEVEL); the symbolic
// STM32_LOG_LEVEL_* values below document the mapping.

#define STM32_LOG_LEVEL_NONE 0
#define STM32_LOG_LEVEL_ERROR 1
#define STM32_LOG_LEVEL_WARN 2
#define STM32_LOG_LEVEL_INFO 3
#define STM32_LOG_LEVEL_DEBUG 4
#define STM32_LOG_LEVEL_TRACE 5

#ifndef STM32_LOG_LEVEL
#define STM32_LOG_LEVEL STM32_LOG_LEVEL_INFO
#endif

// Every macro comes in three forms: bare message, message + decimal uint32
// (_U32), message + 0x-prefixed hex uint32 (_HEX). Each level's block is either
// active or a no-op depending on STM32_LOG_LEVEL, so stripped levels leave no
// trace in the image.
#define STM32_LOG_MSG_(lvl, tag, msg) \
  ::driver::log::detail::logMessage(::driver::log::Level::lvl, (tag), (msg))
#define STM32_LOG_U32_(lvl, tag, msg, val) \
  ::driver::log::detail::logValue(         \
      ::driver::log::Level::lvl,           \
      (tag),                               \
      (msg),                               \
      (val),                               \
      false                                \
  )
#define STM32_LOG_HEX_(lvl, tag, msg, val) \
  ::driver::log::detail::logValue(         \
      ::driver::log::Level::lvl,           \
      (tag),                               \
      (msg),                               \
      (val),                               \
      true                                 \
  )

#if STM32_LOG_LEVEL >= STM32_LOG_LEVEL_ERROR
#define LOG_ERROR(tag, msg) STM32_LOG_MSG_(Error, tag, msg)
#define LOG_ERROR_U32(tag, msg, val) STM32_LOG_U32_(Error, tag, msg, val)
#define LOG_ERROR_HEX(tag, msg, val) STM32_LOG_HEX_(Error, tag, msg, val)
#else
#define LOG_ERROR(tag, msg) ((void) 0)
#define LOG_ERROR_U32(tag, msg, val) ((void) 0)
#define LOG_ERROR_HEX(tag, msg, val) ((void) 0)
#endif

#if STM32_LOG_LEVEL >= STM32_LOG_LEVEL_WARN
#define LOG_WARN(tag, msg) STM32_LOG_MSG_(Warn, tag, msg)
#define LOG_WARN_U32(tag, msg, val) STM32_LOG_U32_(Warn, tag, msg, val)
#define LOG_WARN_HEX(tag, msg, val) STM32_LOG_HEX_(Warn, tag, msg, val)
#else
#define LOG_WARN(tag, msg) ((void) 0)
#define LOG_WARN_U32(tag, msg, val) ((void) 0)
#define LOG_WARN_HEX(tag, msg, val) ((void) 0)
#endif

#if STM32_LOG_LEVEL >= STM32_LOG_LEVEL_INFO
#define LOG_INFO(tag, msg) STM32_LOG_MSG_(Info, tag, msg)
#define LOG_INFO_U32(tag, msg, val) STM32_LOG_U32_(Info, tag, msg, val)
#define LOG_INFO_HEX(tag, msg, val) STM32_LOG_HEX_(Info, tag, msg, val)
#else
#define LOG_INFO(tag, msg) ((void) 0)
#define LOG_INFO_U32(tag, msg, val) ((void) 0)
#define LOG_INFO_HEX(tag, msg, val) ((void) 0)
#endif

#if STM32_LOG_LEVEL >= STM32_LOG_LEVEL_DEBUG
#define LOG_DEBUG(tag, msg) STM32_LOG_MSG_(Debug, tag, msg)
#define LOG_DEBUG_U32(tag, msg, val) STM32_LOG_U32_(Debug, tag, msg, val)
#define LOG_DEBUG_HEX(tag, msg, val) STM32_LOG_HEX_(Debug, tag, msg, val)
#else
#define LOG_DEBUG(tag, msg) ((void) 0)
#define LOG_DEBUG_U32(tag, msg, val) ((void) 0)
#define LOG_DEBUG_HEX(tag, msg, val) ((void) 0)
#endif

#if STM32_LOG_LEVEL >= STM32_LOG_LEVEL_TRACE
#define LOG_TRACE(tag, msg) STM32_LOG_MSG_(Trace, tag, msg)
#define LOG_TRACE_U32(tag, msg, val) STM32_LOG_U32_(Trace, tag, msg, val)
#define LOG_TRACE_HEX(tag, msg, val) STM32_LOG_HEX_(Trace, tag, msg, val)
#else
#define LOG_TRACE(tag, msg) ((void) 0)
#define LOG_TRACE_U32(tag, msg, val) ((void) 0)
#define LOG_TRACE_HEX(tag, msg, val) ((void) 0)
#endif

#endif  // STM32_SDK_DRIVER_LOG_HPP
