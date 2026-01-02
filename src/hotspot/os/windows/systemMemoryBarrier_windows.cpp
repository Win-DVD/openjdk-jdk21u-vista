/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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
#include "systemMemoryBarrier_windows.hpp"

#include <windows.h>
#include <processthreadsapi.h>

// FlushProcessWriteBuffers (i really hope this works) 
typedef struct _FPWB_CTX {
    DWORD_PTR affinity;
    HANDLE done;
    volatile LONG* pending;
    volatile LONG* dummy;
} FPWB_CTX;

static DWORD WINAPI
CompatFlushProcessWriteBuffers_Worker(LPVOID param)
{
    FPWB_CTX* ctx = (FPWB_CTX*)param;

    if (ctx->affinity != 0) {
        SetThreadAffinityMask(GetCurrentThread(), ctx->affinity);
    }

    InterlockedIncrement((volatile LONG*)ctx->dummy);

    if (InterlockedDecrement((volatile LONG*)ctx->pending) == 0) {
        SetEvent(ctx->done);
    }

    return 0;
}

static VOID WINAPI
CompatFlushProcessWriteBuffers(VOID)
{
    typedef VOID (WINAPI *PFN_FlushProcessWriteBuffers)(VOID);

    static PFN_FlushProcessWriteBuffers pFlushProcessWriteBuffers = NULL;
    static LONG initState_FPWB = 0;

    if (InterlockedCompareExchange(&initState_FPWB, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("kERNEL32.DLL"));
        if (hKernel32) {
            pFlushProcessWriteBuffers = (PFN_FlushProcessWriteBuffers)
                GetProcAddress(hKernel32, "FlushProcessWriteBuffers");
        }
        InterlockedExchange(&initState_FPWB, 2);
    } else {
        while (InterlockedCompareExchange(&initState_FPWB, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pFlushProcessWriteBuffers != NULL) {
        pFlushProcessWriteBuffers();
        return;
    }

    DWORD saved_le = GetLastError();

    static volatile LONG dummy = 0;
    InterlockedIncrement(&dummy);

    DWORD_PTR processMask = 0, systemMask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) ||
        processMask == 0 ||
        (processMask & (processMask - 1)) == 0) {
        SetLastError(saved_le);
        return;
    }

    DWORD count = 0;
    for (DWORD_PTR m = processMask; m != 0; m &= (m - 1)) {
        count++;
    }

    HANDLE done = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (done == NULL) {
        SetLastError(saved_le);
        return;
    }

    volatile LONG pending = (LONG)count;

    SIZE_T handles_bytes = (SIZE_T)count * sizeof(HANDLE);
    SIZE_T ctx_bytes     = (SIZE_T)count * sizeof(FPWB_CTX);

    HANDLE* handles = (HANDLE*)HeapAlloc(GetProcessHeap(), 0, handles_bytes);
    FPWB_CTX* ctxs  = (FPWB_CTX*)HeapAlloc(GetProcessHeap(), 0, ctx_bytes);

    if (handles == NULL || ctxs == NULL) {
        if (handles) HeapFree(GetProcessHeap(), 0, handles);
        if (ctxs)    HeapFree(GetProcessHeap(), 0, ctxs);
        CloseHandle(done);
        SetLastError(saved_le);
        return;
    }

    ZeroMemory(handles, handles_bytes);

    DWORD i = 0;
    for (DWORD_PTR m = processMask; m != 0; m &= (m - 1)) {
        DWORD_PTR bit = (m & ~(m - 1));

        ctxs[i].affinity = bit;
        ctxs[i].done = done;
        ctxs[i].pending = &pending;
        ctxs[i].dummy = &dummy;

        handles[i] = CreateThread(NULL, 0, CompatFlushProcessWriteBuffers_Worker, &ctxs[i], 0, NULL);
        if (handles[i] == NULL) {
            if (InterlockedDecrement((volatile LONG*)&pending) == 0) {
                SetEvent(done);
            }
        }

        i++;
    }

    WaitForSingleObject(done, INFINITE);

    for (i = 0; i < count; i++) {
        if (handles[i] != NULL) {
            WaitForSingleObject(handles[i], INFINITE);
            CloseHandle(handles[i]);
        }
    }

    HeapFree(GetProcessHeap(), 0, ctxs);
    HeapFree(GetProcessHeap(), 0, handles);
    CloseHandle(done);

    SetLastError(saved_le);
}
// end FlushProcessWriteBuffers

bool WindowsSystemMemoryBarrier::initialize() {
  return true;
}

void WindowsSystemMemoryBarrier::emit() {
  CompatFlushProcessWriteBuffers();
}
