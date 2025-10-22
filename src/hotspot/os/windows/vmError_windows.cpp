/*
 * Copyright (c) 2003, 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "cds/metaspaceShared.hpp"
#include "runtime/arguments.hpp"
#include "runtime/javaThread.hpp"
#include "runtime/os.hpp"
#include "utilities/vmError.hpp"

// shim for RaiseFailFastException

#ifndef FAIL_FAST_GENERATE_EXCEPTION_ADDRESS
  #define FAIL_FAST_GENERATE_EXCEPTION_ADDRESS 0x0001
#endif
#ifndef FAIL_FAST_NO_HARD_ERROR_DLG
  #define FAIL_FAST_NO_HARD_ERROR_DLG         0x0002
#endif

#ifndef STATUS_STACK_BUFFER_OVERRUN
  #define STATUS_STACK_BUFFER_OVERRUN  ((DWORD)0xC0000409)
#endif
#ifndef STATUS_FAIL_FAST_EXCEPTION
  #define STATUS_FAIL_FAST_EXCEPTION   ((DWORD)0xC0000602)
#endif

typedef VOID (WINAPI *PFN_RaiseFailFastException)(PEXCEPTION_RECORD, PCONTEXT, DWORD);

#ifdef _MSC_VER
  #include <intrin.h>
  #pragma intrinsic(_ReturnAddress)
#endif

static inline VOID RaiseFailFastException_Runtime(PEXCEPTION_RECORD pExceptionRecord,
                                                  PCONTEXT          pContextRecord,
                                                  DWORD             dwFlags)
{
  static PFN_RaiseFailFastException p =
      (PFN_RaiseFailFastException)GetProcAddress(GetModuleHandleA("kernel32.dll"),
                                                 "RaiseFailFastException");
  if (p) {
    p(pExceptionRecord, pContextRecord, dwFlags);
    // If it returns for any reason, make sure we still die:
    TerminateProcess(GetCurrentProcess(), (UINT)-1);
  }

  // XP/Vista

  if (dwFlags & FAIL_FAST_NO_HARD_ERROR_DLG) {
    // prevent the "This program has stopped working" GPF dialog if caller asked
    SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX);
  }

  EXCEPTION_RECORD er;
  if (pExceptionRecord) {
    er = *pExceptionRecord;
    er.ExceptionFlags |= EXCEPTION_NONCONTINUABLE;
    if (!er.ExceptionCode) {
      er.ExceptionCode = STATUS_STACK_BUFFER_OVERRUN;
    }
    if (!er.ExceptionAddress && (dwFlags & FAIL_FAST_GENERATE_EXCEPTION_ADDRESS)) {
#ifdef _MSC_VER
      er.ExceptionAddress = _ReturnAddress();
#else
      er.ExceptionAddress = (PVOID)RaiseFailFastException_Runtime; // best we can do portably
#endif
    }
  } else {
    ZeroMemory(&er, sizeof(er));
    er.ExceptionCode  = STATUS_STACK_BUFFER_OVERRUN; // recognized by WER down-level
    er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#ifdef _MSC_VER
    if (dwFlags & FAIL_FAST_GENERATE_EXCEPTION_ADDRESS) {
      er.ExceptionAddress = _ReturnAddress();
    }
#endif
    // if the caller provided a context with subcode info
    // there is no way to pass it to RaiseException directly. we will ignore it
  }

  // convert EXCEPTION_RECORD params to RaiseException form.
  ULONG_PTR params[EXCEPTION_MAXIMUM_PARAMETERS];
  DWORD nparams = 0;
  if (er.NumberParameters <= EXCEPTION_MAXIMUM_PARAMETERS) {
    nparams = er.NumberParameters;
    for (DWORD i = 0; i < nparams; ++i) params[i] = er.ExceptionInformation[i];
  }

  __try {
    RaiseException(er.ExceptionCode, er.ExceptionFlags, nparams, nparams ? params : NULL);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // if somebody catches it, refuse to continue. this hopefully mirrors fail-fast
  }

  TerminateProcess(GetCurrentProcess(), (UINT)er.ExceptionCode);
  // no return
}

// force all calls in this TU to go through the shim
#ifdef RaiseFailFastException
  #undef RaiseFailFastException
#endif
#define RaiseFailFastException(per, pctx, flg) RaiseFailFastException_Runtime((per), (pctx), (flg))
// end shim

LONG WINAPI crash_handler(struct _EXCEPTION_POINTERS* exceptionInfo) {
  DWORD exception_code = exceptionInfo->ExceptionRecord->ExceptionCode;
  VMError::report_and_die(nullptr, exception_code, nullptr, exceptionInfo->ExceptionRecord,
                          exceptionInfo->ContextRecord);
  return EXCEPTION_CONTINUE_SEARCH;
}

void VMError::install_secondary_signal_handler() {
  SetUnhandledExceptionFilter(crash_handler);
}

// Write a hint to the stream in case siginfo relates to a segv/bus error
// and the offending address points into CDS archive.
void VMError::check_failing_cds_access(outputStream* st, const void* siginfo) {
#if INCLUDE_CDS
  if (siginfo && UseSharedSpaces) {
    const EXCEPTION_RECORD* const er = (const EXCEPTION_RECORD*)siginfo;
    if (er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR &&
        er->NumberParameters >= 2) {
      const void* const fault_addr = (const void*) er->ExceptionInformation[1];
      if (fault_addr != nullptr) {
        if (MetaspaceShared::is_in_shared_metaspace(fault_addr)) {
          st->print("Error accessing class data sharing archive. "
            "Mapped file inaccessible during execution, possible disk/network problem.");
        }
      }
    }
  }
#endif
}

// Error reporting cancellation: there is no easy way to implement this on Windows, because we do
// not have an easy way to send signals to threads (aka to cause a win32 Exception in another
// thread). We would need something like "RaiseException(HANDLE thread)"...
void VMError::reporting_started() {}
void VMError::interrupt_reporting_thread() {}

void VMError::raise_fail_fast(void* exrecord, void* context) {
  DWORD flags = (exrecord == nullptr) ? FAIL_FAST_GENERATE_EXCEPTION_ADDRESS : 0;
  RaiseFailFastException(static_cast<PEXCEPTION_RECORD>(exrecord),
                         static_cast<PCONTEXT>(context),
                         flags);
}
