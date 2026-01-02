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

// RaiseFailFastException
#ifndef FAIL_FAST_GENERATE_EXCEPTION_ADDRESS
#define FAIL_FAST_GENERATE_EXCEPTION_ADDRESS 0x00000001u
#endif

#ifndef STATUS_FAIL_FAST_EXCEPTION
#define STATUS_FAIL_FAST_EXCEPTION ((DWORD)0xC0000409u)
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
static __forceinline void* CompatReturnAddress(void) { return _ReturnAddress(); }
#elif defined(__GNUC__) || defined(__clang__)
static __inline__ void* CompatReturnAddress(void) { return __builtin_return_address(0); }
#else
static __inline__ void* CompatReturnAddress(void) { return NULL; }
#endif

#if defined(_M_IX86) && defined(_MSC_VER)
static __forceinline void CompatCaptureContextX86(PCONTEXT ctx)
{
    ZeroMemory(ctx, sizeof(*ctx));
    ctx->ContextFlags = CONTEXT_CONTROL;
    __asm {
        mov eax, ctx
        mov [eax]CONTEXT.Ebp, ebp
        mov [eax]CONTEXT.Esp, esp
        call $+5
        pop edx
        mov [eax]CONTEXT.Eip, edx
    }
}
#endif

static VOID WINAPI
CompatRaiseFailFastException(PEXCEPTION_RECORD pExceptionRecord,
                            PCONTEXT          pContextRecord,
                            DWORD             dwFlags)
{
    typedef VOID (WINAPI *PFN_RaiseFailFastException)(PEXCEPTION_RECORD, PCONTEXT, DWORD);
    typedef LONG (NTAPI *PFN_NtRaiseException)(PEXCEPTION_RECORD, PCONTEXT, BOOLEAN);
    typedef VOID (WINAPI *PFN_RtlCaptureContext)(PCONTEXT);

    static PFN_RaiseFailFastException pRaiseFailFastException = NULL;
    static PFN_NtRaiseException       pNtRaiseException       = NULL;
    static PFN_RtlCaptureContext      pRtlCaptureContext      = NULL;
    static LONG initState_RFFE = 0;

    if (InterlockedCompareExchange(&initState_RFFE, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pRaiseFailFastException = (PFN_RaiseFailFastException)
                GetProcAddress(hKernel32, "RaiseFailFastException");
            pRtlCaptureContext = (PFN_RtlCaptureContext)
                GetProcAddress(hKernel32, "RtlCaptureContext");
        }
        {
            HMODULE hNtdll = GetModuleHandle(TEXT("NTDLL.DLL"));
            if (hNtdll) {
                pNtRaiseException = (PFN_NtRaiseException)
                    GetProcAddress(hNtdll, "NtRaiseException");
            }
        }
        InterlockedExchange(&initState_RFFE, 2);
    } else {
        while (InterlockedCompareExchange(&initState_RFFE, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pRaiseFailFastException != NULL) {
        pRaiseFailFastException(pExceptionRecord, pContextRecord, dwFlags);
        TerminateProcess(GetCurrentProcess(), STATUS_FAIL_FAST_EXCEPTION);
        return;
    }

    EXCEPTION_RECORD localEr;
    CONTEXT          localCtx;
    PEXCEPTION_RECORD er  = pExceptionRecord ? pExceptionRecord : &localEr;
    PCONTEXT          ctx = pContextRecord   ? pContextRecord   : &localCtx;

    if (pExceptionRecord == NULL) {
        ZeroMemory(&localEr, sizeof(localEr));
        er->ExceptionCode = STATUS_FAIL_FAST_EXCEPTION;
        er->ExceptionFlags = EXCEPTION_NONCONTINUABLE;
        er->NumberParameters = 1;
        er->ExceptionInformation[0] = 0;
    }

    if ((dwFlags & FAIL_FAST_GENERATE_EXCEPTION_ADDRESS) != 0) {
        if (er->ExceptionAddress == NULL) {
            er->ExceptionAddress = CompatReturnAddress();
        }
    }

    if (pContextRecord == NULL) {
        ZeroMemory(&localCtx, sizeof(localCtx));
        if (pRtlCaptureContext != NULL) {
            pRtlCaptureContext(&localCtx);
#if defined(_M_X64)
            if (er->ExceptionAddress != NULL) localCtx.Rip = (DWORD64)(ULONG_PTR)er->ExceptionAddress;
#elif defined(_M_IX86)
            if (er->ExceptionAddress != NULL) localCtx.Eip = (DWORD)(ULONG_PTR)er->ExceptionAddress;
#endif
#if defined(_M_IX86) && defined(_MSC_VER)
        } else {
            CompatCaptureContextX86(&localCtx);
            if (er->ExceptionAddress != NULL) localCtx.Eip = (DWORD)(ULONG_PTR)er->ExceptionAddress;
#endif
        }
    }

    if (pNtRaiseException != NULL && ctx != NULL) {
        pNtRaiseException(er, ctx, FALSE);
    } else {
        __try {
            RaiseException(er->ExceptionCode,
                           EXCEPTION_NONCONTINUABLE,
                           er->NumberParameters,
                           (er->NumberParameters ? (ULONG_PTR*)er->ExceptionInformation : NULL));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    TerminateProcess(GetCurrentProcess(), (UINT)er->ExceptionCode);
}
// end RaiseFailFastException

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
  CompatRaiseFailFastException(static_cast<PEXCEPTION_RECORD>(exrecord),
                         static_cast<PCONTEXT>(context),
                         flags);
}
