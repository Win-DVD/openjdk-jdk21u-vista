/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
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
 */

#include <Windows.h>
#include <malloc.h>
#include <versionhelpers.h>
#include <stdio.h>

// GetActiveProcessorCount
static DWORD WINAPI
CompatGetActiveProcessorCount(WORD GroupNumber)
{
    typedef DWORD (WINAPI *PFN_GetActiveProcessorCount)(WORD);

    static PFN_GetActiveProcessorCount pGetActiveProcessorCount = NULL;
    static LONG initState_GAPC = 0;

    if (InterlockedCompareExchange(&initState_GAPC, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pGetActiveProcessorCount = (PFN_GetActiveProcessorCount)
                GetProcAddress(hKernel32, "GetActiveProcessorCount");
        }
        InterlockedExchange(&initState_GAPC, 2);
    } else {
        while (InterlockedCompareExchange(&initState_GAPC, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pGetActiveProcessorCount != NULL) {
        return pGetActiveProcessorCount(GroupNumber);
    }

    DWORD saved_le = GetLastError();

    if (GroupNumber == ALL_PROCESSOR_GROUPS || GroupNumber == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        SetLastError(saved_le);
        return si.dwNumberOfProcessors;
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
}
// end GetActiveProcessorCount

// GetProcessGroupAffinity
static BOOL WINAPI
CompatGetProcessGroupAffinity(HANDLE hProcess, PUSHORT GroupCount, PUSHORT GroupArray)
{
    typedef BOOL (WINAPI *PFN_GetProcessGroupAffinity)(HANDLE, PUSHORT, PUSHORT);

    static PFN_GetProcessGroupAffinity pGetProcessGroupAffinity = NULL;
    static LONG initState_GPGA = 0;

    if (InterlockedCompareExchange(&initState_GPGA, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
        if (hKernel32) {
            pGetProcessGroupAffinity = (PFN_GetProcessGroupAffinity)
                GetProcAddress(hKernel32, "GetProcessGroupAffinity");
        }
        InterlockedExchange(&initState_GPGA, 2);
    } else {
        while (InterlockedCompareExchange(&initState_GPGA, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pGetProcessGroupAffinity != NULL) {
        return pGetProcessGroupAffinity(hProcess, GroupCount, GroupArray);
    }

    (void)hProcess;

    if (GroupCount == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DWORD saved_le = GetLastError();

    if (GroupArray == NULL || *GroupCount < 1) {
        *GroupCount = 1;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    GroupArray[0] = 0;
    *GroupCount = 1;

    SetLastError(saved_le);
    return TRUE;
}
// end GetProcessGroupAffinity

int main()
{
  DWORD active_processor_count = CompatGetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  if (active_processor_count == 0) {
    printf("GetActiveProcessorCount failed with error: %x\n", GetLastError());
    return 1;
  }

  printf("IsWindowsServer: %d\n", IsWindowsServer() ? 1 : 0);
  printf("Active processor count across all processor groups: %d\n", active_processor_count);

  USHORT group_count = 0;

  if (CompatGetProcessGroupAffinity(GetCurrentProcess(), &group_count, NULL) == 0) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_INSUFFICIENT_BUFFER) {
      if (group_count == 0) {
        printf("Unexpected group count of 0 from GetProcessGroupAffinity.\n");
        return 1;
      }
    } else {
      printf("GetActiveProcessorCount failed with error: %x\n", GetLastError());
      return 1;
    }
  } else {
    printf("Unexpected GetProcessGroupAffinity success result.\n");
    return 1;
  }

  PUSHORT group_array = (PUSHORT)malloc(group_count * sizeof(USHORT));
  if (group_array == NULL) {
    printf("malloc failed.\n");
    return 1;
  }

  printf("Active processors per group: ");
  for (USHORT i=0; i < group_count; i++) {
    DWORD active_processors_in_group = GetActiveProcessorCount(i);
    if (active_processors_in_group == 0) {
      printf("GetActiveProcessorCount(%d) failed with error: %x\n", i, GetLastError());
      return 1;
    }

    printf("%d,", active_processors_in_group);
  }

  free(group_array);
  return 0;
}
