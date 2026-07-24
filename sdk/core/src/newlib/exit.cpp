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

#include <stdlib.h>

#include "diag/trace.h"

// _exit()/abort() are newlib C-ABI hooks and __reset_hardware() is defined by
// the (now C++) cortexm glue with C linkage; keep the whole unit extern "C".
extern "C" {

// ----------------------------------------------------------------------------

#if !defined(DEBUG)
extern void __attribute__((noreturn)) __reset_hardware();
#endif

// ----------------------------------------------------------------------------

// Forward declaration

void _exit(int code);

// ----------------------------------------------------------------------------

// On Release, call the hardware reset procedure.
// On Debug we just enter an infinite loop, to be used as landmark when halting
// the debugger.
//
// It can be redefined in the application, if more functionality
// is required.

void __attribute__((weak)) _exit([[maybe_unused]] int code) {
#if !defined(DEBUG)
  __reset_hardware();
#endif

  // TODO: write on trace
  while (1)
    ;
}

// ----------------------------------------------------------------------------

void __attribute__((weak, noreturn)) abort() {
  trace_puts("abort(), exiting...");

  _exit(1);
}

// ----------------------------------------------------------------------------

}  // extern "C"
