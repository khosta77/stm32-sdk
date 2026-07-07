module;
#include <cstddef>
#include <cstdint>
export module driver.log;

// Portable core of the logging facility (issues #36, #37). CMSIS-free: only
// STL headers in the global module fragment, so this compiles on the host too
// and is exercised by tests/host/test_log.cpp.
//
// Two orthogonal filters keep firmware small and flexible:
//   * Compile-time: the LOG_* macros in driver/log.hpp expand to ((void)0) for
//     levels above STM32_LOG_LEVEL, so their format strings never reach flash.
//   * Runtime: setLevel() gates the calls that survived compilation without a
//     rebuild.
//
// Output is decoupled from formatting through a single installed sink -- a
// captureless thunk (void(*)(void*, const char*, size_t)), the same style as
// system::WorkItem, never std::function. The backend modules (driver.log.itm,
// driver.log.uart) provide concrete sinks selected by the build.
//
// The public entry points and the detail::log* dispatchers the macros call are
// exported but defined out-of-line: the sink pointer and runtime level are
// module-private state, so importers reach them only through these symbols.

export namespace driver::log {

enum class Level : uint8_t {
  None = 0,
  Error = 1,
  Warn = 2,
  Info = 3,
  Debug = 4,
  Trace = 5
};

// A sink consumes an already-formatted byte run. `ctx` carries backend state
// (e.g. the UART object); stateless backends ignore it.
using SinkFn = void (*)(void *ctx, const char *data, size_t len);

// Installs the byte sink used by every subsequent LOG_* call. `ctx` is passed
// back verbatim to `fn`. Call once during startup, before logging.
void setSink(SinkFn fn, void *ctx);

// Sets the runtime threshold: records strictly above `lvl` are dropped.
// Level::None silences everything.
void setLevel(Level lvl);

[[nodiscard]] Level level();

namespace detail {

// Called by the LOG_* macros in driver/log.hpp. Not meant for direct use.
void logMessage(Level lvl, const char *tag, const char *msg);
void logValue(
    Level lvl,
    const char *tag,
    const char *msg,
    uint32_t value,
    bool hex
);

}  // namespace detail

}  // namespace driver::log

// ---- module-private implementation ----------------------------------------

namespace driver::log::detail {

SinkFn g_sink = nullptr;
void *g_ctx = nullptr;
Level g_level = Level::Info;

size_t cstrLen(const char *s) {
  return s == nullptr ? 0 : __builtin_strlen(s);
}

void emit(const char *s) {
  if (g_sink != nullptr && s != nullptr) {
    g_sink(g_ctx, s, cstrLen(s));
  }
}

const char *levelTag(Level lvl) {
  switch (lvl) {
    case Level::Error:
      return "E";
    case Level::Warn:
      return "W";
    case Level::Info:
      return "I";
    case Level::Debug:
      return "D";
    case Level::Trace:
      return "T";
    default:
      return "?";
  }
}

// Writes `value` in base 10 (hex == false) or base 16 (hex == true) into buf,
// returning a pointer to the start of the NUL-terminated digits inside buf.
// buf must hold at least 11 chars (UINT32_MAX decimal, or "0x" + 8 hex digits).
const char *formatUint(char *buf, size_t bufLen, uint32_t value, bool hex) {
  char *p = buf + bufLen;
  *--p = '\0';
  const uint32_t base = hex ? 16U : 10U;
  do {
    const uint32_t digit = value % base;
    *--p = static_cast<char>(digit < 10 ? '0' + digit : 'a' + (digit - 10));
    value /= base;
  } while (value != 0);
  if (hex) {
    *--p = 'x';
    *--p = '0';
  }
  return p;
}

// Drops records below the installed level before any formatting work.
bool runtimeGate(Level lvl) {
  return g_sink != nullptr && g_level != Level::None &&
         static_cast<uint8_t>(lvl) <= static_cast<uint8_t>(g_level);
}

void emitHeader(Level lvl, const char *tag) {
  emit(levelTag(lvl));
  emit(" [");
  emit(tag);
  emit("] ");
}

void logMessage(Level lvl, const char *tag, const char *msg) {
  if (!runtimeGate(lvl)) {
    return;
  }
  emitHeader(lvl, tag);
  emit(msg);
  emit("\r\n");
}

void logValue(
    Level lvl,
    const char *tag,
    const char *msg,
    uint32_t value,
    bool hex
) {
  if (!runtimeGate(lvl)) {
    return;
  }
  char buf[11];
  emitHeader(lvl, tag);
  emit(msg);
  emit(formatUint(buf, sizeof(buf), value, hex));
  emit("\r\n");
}

}  // namespace driver::log::detail

namespace driver::log {

void setSink(SinkFn fn, void *ctx) {
  detail::g_sink = fn;
  detail::g_ctx = ctx;
}

void setLevel(Level lvl) {
  detail::g_level = lvl;
}

Level level() {
  return detail::g_level;
}

}  // namespace driver::log
