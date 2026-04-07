/*
 * Copyright (c) 1997, 2023, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_WINDOWS_OS_WINDOWS_HPP
#define OS_WINDOWS_OS_WINDOWS_HPP

#include "runtime/os.hpp"

// Condition Variable API stuff
typedef struct _COMPAT_CV_STATE {
    CRITICAL_SECTION lock;
    LONG waiters;
    LONG was_broadcast;
    HANDLE sema;
    HANDLE waiters_done;
} COMPAT_CV_STATE;

static COMPAT_CV_STATE*
CompatCvGetState(PCONDITION_VARIABLE cv, BOOL create_if_missing)
{
    PVOID p;
    COMPAT_CV_STATE* s;

    if (cv == NULL) return NULL;

    for (;;) {
        p = InterlockedCompareExchangePointer((PVOID*)&cv->Ptr, NULL, NULL);
        if (p == NULL) break;
        if (p != (PVOID)1) return (COMPAT_CV_STATE*)p;
        SwitchToThread();
    }

    if (!create_if_missing) return NULL;

    if (InterlockedCompareExchangePointer((PVOID*)&cv->Ptr, (PVOID)1, NULL) != NULL) {
        for (;;) {
            p = InterlockedCompareExchangePointer((PVOID*)&cv->Ptr, NULL, NULL);
            if (p != (PVOID)1) break;
            SwitchToThread();
        }
        return (p && p != (PVOID)1) ? (COMPAT_CV_STATE*)p : NULL;
    }

    s = (COMPAT_CV_STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*s));
    if (s == NULL) {
        InterlockedExchangePointer((PVOID*)&cv->Ptr, NULL);
        return NULL;
    }

    InitializeCriticalSection(&s->lock);

    s->sema = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
    s->waiters_done = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (s->sema == NULL || s->waiters_done == NULL) {
        if (s->sema) CloseHandle(s->sema);
        if (s->waiters_done) CloseHandle(s->waiters_done);
        DeleteCriticalSection(&s->lock);
        HeapFree(GetProcessHeap(), 0, s);
        InterlockedExchangePointer((PVOID*)&cv->Ptr, NULL);
        return NULL;
    }

    InterlockedExchangePointer((PVOID*)&cv->Ptr, s);
    return s;
}

static DWORD WINAPI
CompatSleepConditionVariableCS_Worker(LPVOID)
{
    return 0;
}

// InitializeConditionVariable
static VOID WINAPI
CompatInitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{
    typedef VOID (WINAPI *PFN_InitializeConditionVariable)(PCONDITION_VARIABLE);

    static PFN_InitializeConditionVariable pInitializeConditionVariable = NULL;
    static LONG initState_ICV = 0;

    if (InterlockedCompareExchange(&initState_ICV, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pInitializeConditionVariable = (PFN_InitializeConditionVariable)
                GetProcAddress(hKernel32, "InitializeConditionVariable");
        }
        InterlockedExchange(&initState_ICV, 2);
    } else {
        while (InterlockedCompareExchange(&initState_ICV, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pInitializeConditionVariable != NULL) {
        pInitializeConditionVariable(ConditionVariable);
        return;
    }

    if (ConditionVariable != NULL) {
        InterlockedExchangePointer((PVOID*)&ConditionVariable->Ptr, NULL);
    }
}
// end InitializeConditionVariable

// WakeConditionVariable
static VOID WINAPI
CompatWakeConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{
    typedef VOID (WINAPI *PFN_WakeConditionVariable)(PCONDITION_VARIABLE);

    static PFN_WakeConditionVariable pWakeConditionVariable = NULL;
    static LONG initState_WCV = 0;

    if (InterlockedCompareExchange(&initState_WCV, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pWakeConditionVariable = (PFN_WakeConditionVariable)
                GetProcAddress(hKernel32, "WakeConditionVariable");
        }
        InterlockedExchange(&initState_WCV, 2);
    } else {
        while (InterlockedCompareExchange(&initState_WCV, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pWakeConditionVariable != NULL) {
        pWakeConditionVariable(ConditionVariable);
        return;
    }

    COMPAT_CV_STATE* s = CompatCvGetState(ConditionVariable, FALSE);
    if (s == NULL) return;

    EnterCriticalSection(&s->lock);
    LONG have = s->waiters;
    LeaveCriticalSection(&s->lock);

    if (have > 0) {
        ReleaseSemaphore(s->sema, 1, NULL);
    }
}
// end WakeConditionVariable

// WakeAllConditionVariable
static VOID WINAPI
CompatWakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{
    typedef VOID (WINAPI *PFN_WakeAllConditionVariable)(PCONDITION_VARIABLE);

    static PFN_WakeAllConditionVariable pWakeAllConditionVariable = NULL;
    static LONG initState_WACV = 0;

    if (InterlockedCompareExchange(&initState_WACV, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pWakeAllConditionVariable = (PFN_WakeAllConditionVariable)
                GetProcAddress(hKernel32, "WakeAllConditionVariable");
        }
        InterlockedExchange(&initState_WACV, 2);
    } else {
        while (InterlockedCompareExchange(&initState_WACV, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pWakeAllConditionVariable != NULL) {
        pWakeAllConditionVariable(ConditionVariable);
        return;
    }

    COMPAT_CV_STATE* s = CompatCvGetState(ConditionVariable, FALSE);
    if (s == NULL) return;

    EnterCriticalSection(&s->lock);
    LONG n = s->waiters;
    if (n > 0) {
        s->was_broadcast = 1;
        ResetEvent(s->waiters_done);
        ReleaseSemaphore(s->sema, n, NULL);
        LeaveCriticalSection(&s->lock);

        WaitForSingleObject(s->waiters_done, INFINITE);

        EnterCriticalSection(&s->lock);
        s->was_broadcast = 0;
    }
    LeaveCriticalSection(&s->lock);
}
// end WakeAllConditionVariable

// SleepConditionVariableCS
static BOOL WINAPI
CompatSleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable,
                               PCRITICAL_SECTION   CriticalSection,
                               DWORD              dwMilliseconds)
{
    typedef BOOL (WINAPI *PFN_SleepConditionVariableCS)(PCONDITION_VARIABLE, PCRITICAL_SECTION, DWORD);

    static PFN_SleepConditionVariableCS pSleepConditionVariableCS = NULL;
    static LONG initState_SCVCS = 0;

    if (InterlockedCompareExchange(&initState_SCVCS, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pSleepConditionVariableCS = (PFN_SleepConditionVariableCS)
                GetProcAddress(hKernel32, "SleepConditionVariableCS");
        }
        InterlockedExchange(&initState_SCVCS, 2);
    } else {
        while (InterlockedCompareExchange(&initState_SCVCS, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pSleepConditionVariableCS != NULL) {
        return pSleepConditionVariableCS(ConditionVariable, CriticalSection, dwMilliseconds);
    }

    if (ConditionVariable == NULL || CriticalSection == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    COMPAT_CV_STATE* s = CompatCvGetState(ConditionVariable, TRUE);
    if (s == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    EnterCriticalSection(&s->lock);
    s->waiters++;
    LeaveCriticalSection(&s->lock);

    LeaveCriticalSection(CriticalSection);

    DWORD wr = WaitForSingleObject(s->sema, dwMilliseconds);

    EnterCriticalSection(&s->lock);
    s->waiters--;
    BOOL last = (s->was_broadcast != 0 && s->waiters == 0);
    LeaveCriticalSection(&s->lock);

    if (last) {
        SetEvent(s->waiters_done);
    }

    EnterCriticalSection(CriticalSection);

    if (wr == WAIT_OBJECT_0) {
        return TRUE;
    }
    if (wr == WAIT_TIMEOUT) {
        SetLastError(ERROR_TIMEOUT);
        return FALSE;
    }

    return FALSE;
}
// end SleepConditionVariableCS
// end Condition Variable API stuff

// Win32_OS defines the interface to windows operating systems

class outputStream;
class Thread;

typedef void (*signal_handler_t)(int);

class os::win32 {
  friend class os;

 protected:
  static int    _processor_type;
  static int    _processor_level;
  static julong _physical_memory;
  static bool   _is_windows_server;
  static bool   _has_exit_bug;
  static bool   _processor_group_warning_displayed;
  static bool   _job_object_processor_group_warning_displayed;

  static int    _major_version;
  static int    _minor_version;
  static int    _build_number;
  static int    _build_minor;

  static void print_windows_version(outputStream* st);
  static void print_uptime_info(outputStream* st);

  static bool platform_print_native_stack(outputStream* st, const void* context,
                                          char *buf, int buf_size, address& lastpc);

  static bool register_code_area(char *low, char *high);

 public:
  // Windows-specific interface:
  static void   initialize_system_info();
  static void   setmode_streams();
  static bool   is_windows_11_or_greater();
  static bool   is_windows_server_2022_or_greater();
  static bool   request_lock_memory_privilege();
  static size_t large_page_init_decide_size();
  static int windows_major_version() {
    assert(_major_version > 0, "windows version not initialized.");
    return _major_version;
  }
  static int windows_minor_version() {
    assert(_major_version > 0, "windows version not initialized.");
    return _minor_version;
  }
  static int windows_build_number() {
    assert(_major_version > 0, "windows version not initialized.");
    return _build_number;
  }
  static int windows_build_minor() {
    assert(_major_version > 0, "windows version not initialized.");
    return _build_minor;
  }

  static void set_processor_group_warning_displayed(bool displayed)  {
    _processor_group_warning_displayed = displayed;
  }
  static bool processor_group_warning_displayed() {
    return _processor_group_warning_displayed;
  }
  static void set_job_object_processor_group_warning_displayed(bool displayed)  {
    _job_object_processor_group_warning_displayed = displayed;
  }
  static bool job_object_processor_group_warning_displayed() {
    return _job_object_processor_group_warning_displayed;
  }

  // Processor info as provided by NT
  static int processor_type()  { return _processor_type;  }
  static int processor_level() {
    return _processor_level;
  }
  static julong available_memory();
  static julong free_memory();
  static julong physical_memory() { return _physical_memory; }

  // load dll from Windows system directory or Windows directory
  static HINSTANCE load_Windows_dll(const char* name, char *ebuf, int ebuflen);

 private:
  // The handler passed to _beginthreadex().
  // Called with the associated Thread* as the argument.
  static unsigned __stdcall thread_native_entry(void*);

  enum Ept { EPT_THREAD, EPT_PROCESS, EPT_PROCESS_DIE };
  // Wrapper around _endthreadex(), exit() and _exit()
  static int exit_process_or_thread(Ept what, int exit_code);

  static void initialize_performance_counter();
  static void initialize_windows_version();
  static DWORD active_processors_in_job_object(DWORD* active_processor_groups = nullptr);

 public:
  // Generic interface:

  // Tells whether this is a server version of Windows
  static bool is_windows_server() { return _is_windows_server; }

  // Tells whether there can be the race bug during process exit on this platform
  static bool has_exit_bug() { return _has_exit_bug; }

  // Read the headers for the executable that started the current process into
  // the structure passed in (see winnt.h).
  static void read_executable_headers(PIMAGE_NT_HEADERS);

  static bool get_frame_at_stack_banging_point(JavaThread* thread,
                          struct _EXCEPTION_POINTERS* exceptionInfo,
                          address pc, frame* fr);

  struct mapping_info_t {
    // Start of allocation (AllocationBase)
    address base;
    // Total size of allocation over all regions
    size_t size;
    // Total committed size
    size_t committed_size;
    // Number of regions
    int regions;
  };
  // Given an address p which points into an area allocated with VirtualAlloc(),
  // return information about that area.
  static bool find_mapping(address p, mapping_info_t* mapping_info);

#ifndef _WIN64
  // A wrapper to install a structured exception handler for fast JNI accessors.
  static address fast_jni_accessor_wrapper(BasicType);
#endif

  // Fast access to current thread
protected:
  static int _thread_ptr_offset;
private:
  static void initialize_thread_ptr_offset();
public:
  static inline void set_thread_ptr_offset(int offset) {
    _thread_ptr_offset = offset;
  }
  static inline int get_thread_ptr_offset() { return _thread_ptr_offset; }

  // signal support
  static void* install_signal_handler(int sig, signal_handler_t handler);
  static void* user_handler();
};

#endif // OS_WINDOWS_OS_WINDOWS_HPP
