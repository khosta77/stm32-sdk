/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#if defined(TRACE)

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "diag/trace.h"

// ----------------------------------------------------------------------------

// The trace_* portable helpers keep C linkage (declared extern "C" in
// diag/trace.h) so they remain callable from the still-C newlib glue; the
// definitions inherit that linkage from the header declaration.

namespace {

#ifdef OS_INTEGER_TRACE_PRINTF_TMP_ARRAY_SIZE
constexpr std::size_t kTracePrintfBufSize =
    OS_INTEGER_TRACE_PRINTF_TMP_ARRAY_SIZE;
#else
constexpr std::size_t kTracePrintfBufSize = 128;
#endif

}  // namespace

// ----------------------------------------------------------------------------

int trace_printf(const char *format, ...) {
  std::va_list ap;
  va_start(ap, format);

  // TODO: rewrite it to no longer use newlib, it is way too heavy

  static char buf[kTracePrintfBufSize];

  // Print to the local buffer
  int ret = std::vsnprintf(buf, sizeof(buf), format, ap);
  if (ret > 0) {
    // Transfer the buffer to the device
    ret = static_cast<int>(trace_write(buf, static_cast<std::size_t>(ret)));
  }

  va_end(ap);
  return ret;
}

int trace_puts(const char *s) {
  trace_write(s, std::strlen(s));
  return static_cast<int>(trace_write("\n", 1));
}

int trace_putchar(int c) {
  trace_write(reinterpret_cast<const char *>(&c), 1);
  return c;
}

void trace_dump_args(int argc, char *argv[]) {
  trace_printf("main(argc=%d, argv=[", argc);
  for (int i = 0; i < argc; ++i) {
    if (i != 0) {
      trace_printf(", ");
    }
    trace_printf("\"%s\"", argv[i]);
  }
  trace_printf("]);\n");
}

// ----------------------------------------------------------------------------

#endif  // TRACE
