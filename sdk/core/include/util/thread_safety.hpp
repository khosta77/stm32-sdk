#ifndef STM32_SDK_UTIL_THREAD_SAFETY_HPP
#define STM32_SDK_UTIL_THREAD_SAFETY_HPP

// Clang thread-safety annotation wrappers.
//
// Under Clang's -Wthread-safety these attributes drive static analysis: a field
// marked GUARDED_BY(m) may only be touched while capability m is held, a method
// marked REQUIRES(m) must be called with m held, and so on. Under every other
// compiler -- the SDK ships on arm-none-eabi GCC 15, which does not understand
// them -- each macro expands to nothing and the annotated code is unchanged.
//
// This is a purely preprocessor header: it defines no C++ symbols and pulls in
// no FreeRTOS or CMSIS headers, so it is safe to include even from the
// RTOS-free system.work_queue module without adding any runtime dependency.
//
// Today this is groundwork -- the annotations must at minimum compile cleanly
// under GCC -Werror. Wiring the actual Clang analysis into CI lands with
// clang-tidy (issue #73); the capability model here is what it will check.

#if defined(__clang__)
#define STM32_TSA(x) __attribute__((x))
#else
#define STM32_TSA(x)
#endif

#define CAPABILITY(x) STM32_TSA(capability(x))
#define SCOPED_CAPABILITY STM32_TSA(scoped_lockable)
#define GUARDED_BY(x) STM32_TSA(guarded_by(x))
#define PT_GUARDED_BY(x) STM32_TSA(pt_guarded_by(x))
#define ACQUIRED_BEFORE(...) STM32_TSA(acquired_before(__VA_ARGS__))
#define ACQUIRED_AFTER(...) STM32_TSA(acquired_after(__VA_ARGS__))
#define REQUIRES(...) STM32_TSA(requires_capability(__VA_ARGS__))
#define REQUIRES_SHARED(...) STM32_TSA(requires_shared_capability(__VA_ARGS__))
#define ACQUIRE(...) STM32_TSA(acquire_capability(__VA_ARGS__))
#define ACQUIRE_SHARED(...) STM32_TSA(acquire_shared_capability(__VA_ARGS__))
#define RELEASE(...) STM32_TSA(release_capability(__VA_ARGS__))
#define RELEASE_SHARED(...) STM32_TSA(release_shared_capability(__VA_ARGS__))
#define TRY_ACQUIRE(...) STM32_TSA(try_acquire_capability(__VA_ARGS__))
#define EXCLUDES(...) STM32_TSA(locks_excluded(__VA_ARGS__))
#define RETURN_CAPABILITY(x) STM32_TSA(lock_returned(x))
#define NO_THREAD_SAFETY_ANALYSIS STM32_TSA(no_thread_safety_analysis)

#endif  // STM32_SDK_UTIL_THREAD_SAFETY_HPP
