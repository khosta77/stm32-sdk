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

#include "cmsis/cmsis_device.h"
#include "diag/trace.h"

// ----------------------------------------------------------------------------

// One of these definitions must be passed via the compiler command line
// Note: small Cortex-M0/M0+ might implement a simplified debug interface.

// #define OS_USE_TRACE_ITM
// #define OS_USE_TRACE_SEMIHOSTING_DEBUG
// #define OS_USE_TRACE_SEMIHOSTING_STDOUT

#if !(defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__))
#if defined(OS_USE_TRACE_ITM)
#undef OS_USE_TRACE_ITM
#warning "ITM unavailable"
#endif  // defined(OS_USE_TRACE_ITM)
#endif  // !(defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__))

#if defined(OS_DEBUG_SEMIHOSTING_FAULTS)
#if defined(OS_USE_TRACE_SEMIHOSTING_STDOUT) || \
    defined(OS_USE_TRACE_SEMIHOSTING_DEBUG)
#error "Cannot debug semihosting using semihosting trace; use OS_USE_TRACE_ITM"
#endif
#endif

// ----------------------------------------------------------------------------

// Forward definitions of the file-local backends.

namespace {

#if defined(OS_USE_TRACE_ITM)
ssize_t trace_write_itm(const char *buf, size_t nbyte);
#endif

#if defined(OS_USE_TRACE_SEMIHOSTING_STDOUT)
ssize_t trace_write_semihosting_stdout(const char *buf, size_t nbyte);
#endif

#if defined(OS_USE_TRACE_SEMIHOSTING_DEBUG)
ssize_t trace_write_semihosting_debug(const char *buf, size_t nbyte);
#endif

}  // namespace

// ----------------------------------------------------------------------------

void trace_initialize() {
  // For regular ITM / semihosting, no inits required.
}

// ----------------------------------------------------------------------------

// This function is called from _write() for fd==1 or fd==2 and from some
// of the trace_* functions.

ssize_t
trace_write([[maybe_unused]] const char *buf, [[maybe_unused]] size_t nbyte) {
#if defined(OS_USE_TRACE_ITM)
  return trace_write_itm(buf, nbyte);
#elif defined(OS_USE_TRACE_SEMIHOSTING_STDOUT)
  return trace_write_semihosting_stdout(buf, nbyte);
#elif defined(OS_USE_TRACE_SEMIHOSTING_DEBUG)
  return trace_write_semihosting_debug(buf, nbyte);
#endif

  return -1;
}

// ----------------------------------------------------------------------------

#if defined(OS_USE_TRACE_ITM)

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

// ITM is the ARM standard mechanism, running over SWD/SWO on Cortex-M3/M4
// devices, and is the recommended setting, if available.
//
// The JLink probe and the GDB server fully support SWD/SWO
// and the JLink Debugging plug-in enables it by default.
// The current OpenOCD does not include support to parse the SWO stream,
// so this configuration will not work on OpenOCD (will not crash, but
// nothing will be displayed in the output console).

#if !defined(OS_INTEGER_TRACE_ITM_STIMULUS_PORT)
#define OS_INTEGER_TRACE_ITM_STIMULUS_PORT (0)
#endif

namespace {

ssize_t trace_write_itm(const char *buf, size_t nbyte) {
  for (size_t i = 0; i < nbyte; i++) {
    // Check if ITM or the stimulus port are not enabled
    if (((ITM->TCR & ITM_TCR_ITMENA_Msk) == 0) ||
        ((ITM->TER & (1UL << OS_INTEGER_TRACE_ITM_STIMULUS_PORT)) == 0)) {
      return static_cast<ssize_t>(i);  // number of sent characters (may be 0)
    }

    // Wait until STIMx is ready...
    while (ITM->PORT[OS_INTEGER_TRACE_ITM_STIMULUS_PORT].u32 == 0) {
    }
    // then send data, one byte at a time
    ITM->PORT[OS_INTEGER_TRACE_ITM_STIMULUS_PORT].u8 =
        static_cast<uint8_t>(*buf++);
  }

  return static_cast<ssize_t>(nbyte);  // all characters successfully sent
}

}  // namespace

#endif  // defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

#endif  // OS_USE_TRACE_ITM

// ----------------------------------------------------------------------------

#if defined(OS_USE_TRACE_SEMIHOSTING_DEBUG) || \
    defined(OS_USE_TRACE_SEMIHOSTING_STDOUT)

#include "arm/semihosting.h"

// Semihosting is the other output channel that can be used for the trace
// messages. It comes in two flavours: STDOUT and DEBUG. The STDOUT channel
// is the equivalent of the stdout in POSIX and in most cases it is forwarded
// to the GDB server stdout stream. The debug channel is a separate
// channel. STDOUT is buffered, so nothing is displayed until a \n;
// DEBUG is not buffered, but can be slow.
//
// Choosing between semihosting stdout and debug depends on the capabilities
// of your GDB server, and also on specific needs. It is recommended to test
// DEBUG first, and if too slow, try STDOUT.
//
// The JLink GDB server fully support semihosting, and both configurations
// are available; to activate it, use "monitor semihosting enable" or check
// the corresponding button in the JLink Debugging plug-in.
// In OpenOCD, support for semihosting can be enabled using
// "monitor arm semihosting enable".
//
// Note: Applications built with semihosting output active normally cannot
// be executed without the debugger connected and active, since they use
// BKPT to communicate with the host. However, with a carefully written
// HardFault_Handler, the semihosting BKPT calls can be processed, making
// possible to run semihosting applications as standalone, without being
// terminated with hardware faults.

#endif  // OS_USE_TRACE_SEMIHOSTING_DEBUG_*

// ----------------------------------------------------------------------------

#if defined(OS_USE_TRACE_SEMIHOSTING_STDOUT)

namespace {

ssize_t trace_write_semihosting_stdout(const char *buf, size_t nbyte) {
  static int handle;
  void *block[3];
  int ret;

  if (handle == 0) {
    // On the first call get the file handle from the host
    block[0] = const_cast<char *>(":tt");    // filename for stdin/out/err
    block[1] = reinterpret_cast<void *>(4);  // mode "w"
    // length of ":tt", except null terminator
    block[2] = reinterpret_cast<void *>(sizeof(":tt") - 1);

    ret = call_host(SEMIHOSTING_SYS_OPEN, static_cast<void *>(block));
    if (ret == -1)
      return -1;

    handle = ret;
  }

  block[0] = reinterpret_cast<void *>(handle);
  block[1] = const_cast<char *>(buf);
  block[2] = reinterpret_cast<void *>(nbyte);
  // send character array to host file/device
  ret = call_host(SEMIHOSTING_SYS_WRITE, static_cast<void *>(block));
  // this call returns the number of bytes NOT written (0 if all ok)

  // -1 is not a legal value, but SEGGER seems to return it
  if (ret == -1)
    return -1;

  // The compliant way of returning errors
  if (ret == static_cast<int>(nbyte))
    return -1;

  // Return the number of bytes written
  return static_cast<ssize_t>(nbyte) - static_cast<ssize_t>(ret);
}

}  // namespace

#endif  // OS_USE_TRACE_SEMIHOSTING_STDOUT

// ----------------------------------------------------------------------------

#if defined(OS_USE_TRACE_SEMIHOSTING_DEBUG)

#define OS_INTEGER_TRACE_TMP_ARRAY_SIZE (16)

namespace {

ssize_t trace_write_semihosting_debug(const char *buf, size_t nbyte) {
  // Since the single character debug channel is quite slow, try to
  // optimise and send a null terminated string, if possible.
  if (buf[nbyte] == '\0') {
    // send string
    call_host(SEMIHOSTING_SYS_WRITE0, const_cast<char *>(buf));
  } else {
    // If not, use a local buffer to speed things up
    char tmp[OS_INTEGER_TRACE_TMP_ARRAY_SIZE];
    size_t togo = nbyte;
    while (togo > 0) {
      unsigned int n = ((togo < sizeof(tmp)) ? togo : sizeof(tmp));
      unsigned int i = 0;
      for (; i < n; ++i, ++buf) {
        tmp[i] = *buf;
      }
      tmp[i] = '\0';

      call_host(SEMIHOSTING_SYS_WRITE0, tmp);

      togo -= n;
    }
  }

  // All bytes written
  return static_cast<ssize_t>(nbyte);
}

}  // namespace

#endif  // OS_USE_TRACE_SEMIHOSTING_DEBUG

#endif  // TRACE

// ----------------------------------------------------------------------------
