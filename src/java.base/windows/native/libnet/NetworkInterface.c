/*
 * Copyright (c) 2000, 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
#include "net_util.h"
#include "NetworkInterface.h"

#include "java_net_InetAddress.h"
#include "java_net_NetworkInterface.h"

// ConvertLengthToIpv4Mask
static ULONG WINAPI
CompatConvertLengthToIpv4Mask(ULONG MaskLength, PULONG Mask)
{
    typedef ULONG (WINAPI *PFN_ConvertLengthToIpv4Mask)(ULONG, PULONG);

    static PFN_ConvertLengthToIpv4Mask pConvertLengthToIpv4Mask = NULL;
    static LONG initState_CLT4M = 0;

    if (InterlockedCompareExchange(&initState_CLT4M, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pConvertLengthToIpv4Mask = (PFN_ConvertLengthToIpv4Mask)
                GetProcAddress(hIphlpapi, "ConvertLengthToIpv4Mask");
        }
        InterlockedExchange(&initState_CLT4M, 2);
    } else {
        while (InterlockedCompareExchange(&initState_CLT4M, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pConvertLengthToIpv4Mask != NULL) {
        return pConvertLengthToIpv4Mask(MaskLength, Mask);
    }

    if (Mask == NULL || MaskLength > 32) {
        return ERROR_INVALID_PARAMETER;
    }

    ULONG m;
    if (MaskLength == 0) {
        m = 0;
    } else {
        m = 0xFFFFFFFFUL << (32 - MaskLength);
    }

    *Mask = htonl(m);
    return NO_ERROR;
}
// end ConvertLengthToIpv4Mask

// ConvertInterfaceLuidToNameW
static ULONG WINAPI
CompatConvertInterfaceLuidToNameW(const NET_LUID *InterfaceLuid,
                                  PWCHAR InterfaceName,
                                  SIZE_T Length)
{
    typedef ULONG (WINAPI *PFN_ConvertInterfaceLuidToNameW)(const NET_LUID*, PWCHAR, SIZE_T);

    static PFN_ConvertInterfaceLuidToNameW pConvertInterfaceLuidToNameW = NULL;
    static LONG initState_CILTN = 0;

    if (InterlockedCompareExchange(&initState_CILTN, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pConvertInterfaceLuidToNameW = (PFN_ConvertInterfaceLuidToNameW)
                GetProcAddress(hIphlpapi, "ConvertInterfaceLuidToNameW");
        }
        InterlockedExchange(&initState_CILTN, 2);
    } else {
        while (InterlockedCompareExchange(&initState_CILTN, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pConvertInterfaceLuidToNameW != NULL) {
        return pConvertInterfaceLuidToNameW(InterfaceLuid, InterfaceName, Length);
    }

    if (InterfaceLuid == NULL || InterfaceName == NULL || Length == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    DWORD ifIndex = (DWORD)InterfaceLuid->Value;
    if (ifIndex == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    WCHAR tmp[32];
    int n = _snwprintf(tmp, (int)(sizeof(tmp) / sizeof(tmp[0])), L"if%lu", (unsigned long)ifIndex);
    if (n < 0) {
        return ERROR_GEN_FAILURE;
    }

    SIZE_T need = (SIZE_T)n + 1;
    if (Length < need) {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    memcpy(InterfaceName, tmp, need * sizeof(WCHAR));
    return ERROR_SUCCESS;
}
// end ConvertInterfaceLuidToNameW

// ConvertInterfaceNameToLuidW
static ULONG WINAPI
CompatConvertInterfaceNameToLuidW(PCWSTR InterfaceName,
                                  PNET_LUID InterfaceLuid)
{
    typedef ULONG (WINAPI *PFN_ConvertInterfaceNameToLuidW)(PCWSTR, PNET_LUID);

    static PFN_ConvertInterfaceNameToLuidW pConvertInterfaceNameToLuidW = NULL;
    static LONG initState_CINTL = 0;

    if (InterlockedCompareExchange(&initState_CINTL, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pConvertInterfaceNameToLuidW = (PFN_ConvertInterfaceNameToLuidW)
                GetProcAddress(hIphlpapi, "ConvertInterfaceNameToLuidW");
        }
        InterlockedExchange(&initState_CINTL, 2);
    } else {
        while (InterlockedCompareExchange(&initState_CINTL, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pConvertInterfaceNameToLuidW != NULL) {
        return pConvertInterfaceNameToLuidW(InterfaceName, InterfaceLuid);
    }

    if (InterfaceName == NULL || InterfaceLuid == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (InterfaceName[0] != L'i' || InterfaceName[1] != L'f') {
        return ERROR_INVALID_NAME;
    }

    WCHAR* endp = NULL;
    unsigned long v = wcstoul(InterfaceName + 2, &endp, 10);
    if (endp == (InterfaceName + 2) || endp == NULL || *endp != L'\0' || v == 0 || v > 0xFFFFFFFFUL) {
        return ERROR_INVALID_NAME;
    }

    InterfaceLuid->Value = (ULONG64)(DWORD)v;
    return ERROR_SUCCESS;
}
// end ConvertInterfaceNameToLuidW

// FreeMibTable
static VOID WINAPI
CompatFreeMibTable(PVOID Memory)
{
    typedef VOID (WINAPI *PFN_FreeMibTable)(PVOID);

    static PFN_FreeMibTable pFreeMibTable = NULL;
    static LONG initState_FMT = 0;

    if (InterlockedCompareExchange(&initState_FMT, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pFreeMibTable = (PFN_FreeMibTable)
                GetProcAddress(hIphlpapi, "FreeMibTable");
        }
        InterlockedExchange(&initState_FMT, 2);
    } else {
        while (InterlockedCompareExchange(&initState_FMT, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pFreeMibTable != NULL) {
        pFreeMibTable(Memory);
        return;
    }

    if (Memory != NULL) {
        HeapFree(GetProcessHeap(), 0, Memory);
    }
}
// end FreeMibTable

// GetIfEntry2
static ULONG WINAPI
CompatGetIfEntry2(PMIB_IF_ROW2 Row)
{
    typedef ULONG (WINAPI *PFN_GetIfEntry2)(PMIB_IF_ROW2);

    static PFN_GetIfEntry2 pGetIfEntry2 = NULL;
    static LONG initState_GIE2 = 0;

    if (InterlockedCompareExchange(&initState_GIE2, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pGetIfEntry2 = (PFN_GetIfEntry2)
                GetProcAddress(hIphlpapi, "GetIfEntry2");
        }
        InterlockedExchange(&initState_GIE2, 2);
    } else {
        while (InterlockedCompareExchange(&initState_GIE2, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pGetIfEntry2 != NULL) {
        return pGetIfEntry2(Row);
    }

    if (Row == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    DWORD ifIndex = Row->InterfaceIndex;
    if (ifIndex == 0 && Row->InterfaceLuid.Value != 0) {
        ifIndex = (DWORD)Row->InterfaceLuid.Value;
    }
    if (ifIndex == 0) {
        return ERROR_FILE_NOT_FOUND;
    }

    MIB_IFROW ifr;
    ZeroMemory(&ifr, sizeof(ifr));
    ifr.dwIndex = ifIndex;

    ULONG r = GetIfEntry(&ifr);
    if (r != NO_ERROR) {
        return r;
    }

    MIB_IF_ROW2 out;
    ZeroMemory(&out, sizeof(out));

    out.InterfaceIndex = ifIndex;
    out.InterfaceLuid.Value = (ULONG64)ifIndex;

    out.Type = ifr.dwType;
    out.Mtu = ifr.dwMtu;

    out.AdminStatus = (NET_IF_ADMIN_STATUS)ifr.dwAdminStatus;
    out.OperStatus = (IF_OPER_STATUS)ifr.dwOperStatus;

    if (ifr.dwType == IF_TYPE_PPP || ifr.dwType == IF_TYPE_SLIP) {
        out.AccessType = NET_IF_ACCESS_POINT_TO_POINT;
    } else {
        out.AccessType = NET_IF_ACCESS_BROADCAST;
    }

    out.PhysicalAddressLength = ifr.dwPhysAddrLen;
    if (out.PhysicalAddressLength > IF_MAX_PHYS_ADDRESS_LENGTH) {
        out.PhysicalAddressLength = IF_MAX_PHYS_ADDRESS_LENGTH;
    }
    memcpy(out.PhysicalAddress, ifr.bPhysAddr, out.PhysicalAddressLength);

    if (ifr.dwDescrLen != 0) {
        int w = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)ifr.bDescr, (int)ifr.dwDescrLen,
                                    out.Description, NDIS_IF_MAX_STRING_SIZE);
        if (w < 0) w = 0;
        out.Description[(w < NDIS_IF_MAX_STRING_SIZE) ? w : NDIS_IF_MAX_STRING_SIZE] = 0;
    } else {
        out.Description[0] = 0;
    }

    *Row = out;
    return NO_ERROR;
}
// end GetIfEntry2

// GetIfTable2
static ULONG WINAPI
CompatGetIfTable2(PMIB_IF_TABLE2 *Table)
{
    typedef ULONG (WINAPI *PFN_GetIfTable2)(PMIB_IF_TABLE2*);

    static PFN_GetIfTable2 pGetIfTable2 = NULL;
    static LONG initState_GIT2 = 0;

    if (InterlockedCompareExchange(&initState_GIT2, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pGetIfTable2 = (PFN_GetIfTable2)
                GetProcAddress(hIphlpapi, "GetIfTable2");
        }
        InterlockedExchange(&initState_GIT2, 2);
    } else {
        while (InterlockedCompareExchange(&initState_GIT2, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pGetIfTable2 != NULL) {
        return pGetIfTable2(Table);
    }

    if (Table == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    DWORD sz = 0;
    ULONG r = GetIfTable(NULL, &sz, FALSE);
    if (r != ERROR_INSUFFICIENT_BUFFER || sz == 0) {
        return (r == NO_ERROR) ? ERROR_GEN_FAILURE : r;
    }

    MIB_IFTABLE* old = (MIB_IFTABLE*)HeapAlloc(GetProcessHeap(), 0, sz);
    if (old == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    r = GetIfTable(old, &sz, FALSE);
    if (r != NO_ERROR) {
        HeapFree(GetProcessHeap(), 0, old);
        return r;
    }

    ULONG n = old->dwNumEntries;
    SIZE_T outSz;
    if (n == 0) {
        outSz = sizeof(MIB_IF_TABLE2);
    } else {
        outSz = sizeof(MIB_IF_TABLE2) + (SIZE_T)(n - 1) * sizeof(MIB_IF_ROW2);
    }

    MIB_IF_TABLE2* out = (MIB_IF_TABLE2*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, outSz);
    if (out == NULL) {
        HeapFree(GetProcessHeap(), 0, old);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    out->NumEntries = n;

    for (ULONG i = 0; i < n; i++) {
        const MIB_IFROW* ifr = &old->table[i];
        MIB_IF_ROW2* row = &out->Table[i];

        row->InterfaceIndex = ifr->dwIndex;
        row->InterfaceLuid.Value = (ULONG64)ifr->dwIndex;

        row->Type = ifr->dwType;
        row->Mtu = ifr->dwMtu;

        row->AdminStatus = (NET_IF_ADMIN_STATUS)ifr->dwAdminStatus;
        row->OperStatus = (IF_OPER_STATUS)ifr->dwOperStatus;

        if (ifr->dwType == IF_TYPE_PPP || ifr->dwType == IF_TYPE_SLIP) {
            row->AccessType = NET_IF_ACCESS_POINT_TO_POINT;
        } else {
            row->AccessType = NET_IF_ACCESS_BROADCAST;
        }

        row->PhysicalAddressLength = ifr->dwPhysAddrLen;
        if (row->PhysicalAddressLength > IF_MAX_PHYS_ADDRESS_LENGTH) {
            row->PhysicalAddressLength = IF_MAX_PHYS_ADDRESS_LENGTH;
        }
        memcpy(row->PhysicalAddress, ifr->bPhysAddr, row->PhysicalAddressLength);

        if (ifr->dwDescrLen != 0) {
            int w = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)ifr->bDescr, (int)ifr->dwDescrLen,
                                        row->Description, NDIS_IF_MAX_STRING_SIZE);
            if (w < 0) w = 0;
            row->Description[(w < NDIS_IF_MAX_STRING_SIZE) ? w : NDIS_IF_MAX_STRING_SIZE] = 0;
        } else {
            row->Description[0] = 0;
        }
    }

    HeapFree(GetProcessHeap(), 0, old);
    *Table = out;
    return NO_ERROR;
}
// end GetIfTable2

// GetUnicastIpAddressTable
static ULONG WINAPI
CompatGetUnicastIpAddressTable(ADDRESS_FAMILY Family,
                               PMIB_UNICASTIPADDRESS_TABLE *Table)
{
    typedef ULONG (WINAPI *PFN_GetUnicastIpAddressTable)(ADDRESS_FAMILY, PMIB_UNICASTIPADDRESS_TABLE*);

    static PFN_GetUnicastIpAddressTable pGetUnicastIpAddressTable = NULL;
    static LONG initState_GUIAT = 0;

    if (InterlockedCompareExchange(&initState_GUIAT, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pGetUnicastIpAddressTable = (PFN_GetUnicastIpAddressTable)
                GetProcAddress(hIphlpapi, "GetUnicastIpAddressTable");
        }
        InterlockedExchange(&initState_GUIAT, 2);
    } else {
        while (InterlockedCompareExchange(&initState_GUIAT, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pGetUnicastIpAddressTable != NULL) {
        return pGetUnicastIpAddressTable(Family, Table);
    }

    if (Table == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (Family == AF_INET6) {
        return ERROR_NOT_SUPPORTED;
    }

    DWORD sz = 0;
    ULONG r = GetIpAddrTable(NULL, &sz, FALSE);
    if (r != ERROR_INSUFFICIENT_BUFFER || sz == 0) {
        return (r == NO_ERROR) ? ERROR_GEN_FAILURE : r;
    }

    MIB_IPADDRTABLE* ipt = (MIB_IPADDRTABLE*)HeapAlloc(GetProcessHeap(), 0, sz);
    if (ipt == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    r = GetIpAddrTable(ipt, &sz, FALSE);
    if (r != NO_ERROR) {
        HeapFree(GetProcessHeap(), 0, ipt);
        return r;
    }

    ULONG n = ipt->dwNumEntries;
    SIZE_T outSz;
    if (n == 0) {
        outSz = sizeof(MIB_UNICASTIPADDRESS_TABLE);
    } else {
        outSz = sizeof(MIB_UNICASTIPADDRESS_TABLE) + (SIZE_T)(n - 1) * sizeof(MIB_UNICASTIPADDRESS_ROW);
    }

    MIB_UNICASTIPADDRESS_TABLE* out =
        (MIB_UNICASTIPADDRESS_TABLE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, outSz);
    if (out == NULL) {
        HeapFree(GetProcessHeap(), 0, ipt);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    out->NumEntries = n;

    for (ULONG i = 0; i < n; i++) {
        MIB_UNICASTIPADDRESS_ROW* row = &out->Table[i];
        const MIB_IPADDRROW* ir = &ipt->table[i];

        row->InterfaceLuid.Value = (ULONG64)ir->dwIndex;
        row->DadState = IpDadStatePreferred;

        row->Address.si_family = AF_INET;
        row->Address.Ipv4.sin_family = AF_INET;
        row->Address.Ipv4.sin_addr.s_addr = ir->dwAddr;

        ULONG m = ntohl(ir->dwMask);
        UCHAR prefix = 0;
        while (prefix < 32) {
            if ((m & 0x80000000UL) == 0) break;
            prefix++;
            m <<= 1;
        }
        row->OnLinkPrefixLength = prefix;
    }

    HeapFree(GetProcessHeap(), 0, ipt);
    *Table = out;
    return NO_ERROR;
}
// end GetUnicastIpAddressTable

// GetAnycastIpAddressTable
static ULONG WINAPI
CompatGetAnycastIpAddressTable(ADDRESS_FAMILY Family,
                               PMIB_ANYCASTIPADDRESS_TABLE *Table)
{
    typedef ULONG (WINAPI *PFN_GetAnycastIpAddressTable)(ADDRESS_FAMILY, PMIB_ANYCASTIPADDRESS_TABLE*);

    static PFN_GetAnycastIpAddressTable pGetAnycastIpAddressTable = NULL;
    static LONG initState_GAIAT = 0;

    if (InterlockedCompareExchange(&initState_GAIAT, 1, 0) == 0) {
        HMODULE hIphlpapi = GetModuleHandle(TEXT("IPHLPAPI.DLL"));
        if (hIphlpapi) {
            pGetAnycastIpAddressTable = (PFN_GetAnycastIpAddressTable)
                GetProcAddress(hIphlpapi, "GetAnycastIpAddressTable");
        }
        InterlockedExchange(&initState_GAIAT, 2);
    } else {
        while (InterlockedCompareExchange(&initState_GAIAT, 2, 2) != 2) {
            SwitchToThread();
        }
    }

    if (pGetAnycastIpAddressTable != NULL) {
        return pGetAnycastIpAddressTable(Family, Table);
    }

    (void)Family;

    if (Table == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    MIB_ANYCASTIPADDRESS_TABLE* out =
        (MIB_ANYCASTIPADDRESS_TABLE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                               sizeof(MIB_ANYCASTIPADDRESS_TABLE));
    if (out == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    out->NumEntries = 0;
    *Table = out;
    return NO_ERROR;
}
// end GetAnycastIpAddressTable

/*
 * Windows implementation of the java.net.NetworkInterface native methods.
 * This module provides the implementations of getAll, getByName, getByIndex,
 * and getByAddress.
 */

#define NDIS_IF_MAX_BUFFER_SIZE NDIS_IF_MAX_STRING_SIZE + 1
#define NO_PREFIX 255

/* various JNI ids */

jclass ni_class;            /* NetworkInterface */

jmethodID ni_ctor;          /* NetworkInterface() */

jfieldID ni_indexID;        /* NetworkInterface.index */
jfieldID ni_addrsID;        /* NetworkInterface.addrs */
jfieldID ni_bindsID;        /* NetworkInterface.bindings */
jfieldID ni_nameID;         /* NetworkInterface.name */
jfieldID ni_displayNameID;  /* NetworkInterface.displayName */
jfieldID ni_childsID;       /* NetworkInterface.childs */

jclass ni_ibcls;            /* InterfaceAddress */
jmethodID ni_ibctrID;       /* InterfaceAddress() */
jfieldID ni_ibaddressID;        /* InterfaceAddress.address */
jfieldID ni_ibbroadcastID;      /* InterfaceAddress.broadcast */
jfieldID ni_ibmaskID;           /* InterfaceAddress.maskLength */

/*
 * Gets the unicast and anycast IP address tables.
 * If an error occurs while fetching a table,
 * any tables already fetched are freed and an exception is set.
 * It is the caller's responsibility to free the tables when they are no longer needed.
 */
static BOOL getAddressTables(
        JNIEnv *env, MIB_UNICASTIPADDRESS_TABLE **uniAddrs,
        MIB_ANYCASTIPADDRESS_TABLE **anyAddrs) {
    ULONG apiRetVal;
    ADDRESS_FAMILY addrFamily = ipv6_available() ? AF_UNSPEC : AF_INET;

    apiRetVal = CompatGetUnicastIpAddressTable(addrFamily, uniAddrs);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetUnicastIpAddressTable");
        return FALSE;
    }
    apiRetVal = CompatGetAnycastIpAddressTable(addrFamily, anyAddrs);
    if (apiRetVal != NO_ERROR) {
        CompatFreeMibTable(*uniAddrs);
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetAnycastIpAddressTable");
        return FALSE;
    }
    return TRUE;
}

/*
 * Frees a linked list of netaddr structs.
 */
static void freeNetaddrs(netaddr *netaddrP) {
    netaddr *curr = netaddrP;
    while (curr != NULL) {
        netaddrP = netaddrP->Next;
        free(curr);
        curr = netaddrP;
    }
}

/*
 * Builds and returns a java.net.NetworkInterface object from the given MIB_IF_ROW2.
 * Unlike createNetworkInterfaceForSingleRowWithTables,
 * this expects that the row is already populated, either by GetIfEntry2 or GetIfTable2.
 * If anything goes wrong, an exception will be set,
 * but the address tables are not freed.
 * Freeing the address tables is always the caller's responsibility.
 */
static jobject createNetworkInterface(
        JNIEnv *env, MIB_IF_ROW2 *ifRow, MIB_UNICASTIPADDRESS_TABLE *uniAddrs,
        MIB_ANYCASTIPADDRESS_TABLE *anyAddrs) {
    WCHAR ifName[NDIS_IF_MAX_BUFFER_SIZE];
    jobject netifObj, name, displayName, inetAddr, bcastAddr, bindAddr;
    jobjectArray addrArr, bindsArr, childArr;
    netaddr *addrsHead = NULL, *addrsCurrent = NULL;
    int addrCount = 0;
    ULONG apiRetVal, i, mask;

    // instantiate the NetworkInterface object
    netifObj = (*env)->NewObject(env, ni_class, ni_ctor);
    if (netifObj == NULL) {
        return NULL;
    }

    // set the NetworkInterface's name
    apiRetVal = CompatConvertInterfaceLuidToNameW(
            &(ifRow->InterfaceLuid), ifName, NDIS_IF_MAX_BUFFER_SIZE);
    if (apiRetVal != ERROR_SUCCESS) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "ConvertInterfaceLuidToNameW");
        return NULL;
    }
    name = (*env)->NewString(env, ifName, (jsize) wcslen(ifName));
    if (name == NULL) {
        return NULL;
    }
    (*env)->SetObjectField(env, netifObj, ni_nameID, name);
    (*env)->DeleteLocalRef(env, name);

    // set the NetworkInterface's display name
    displayName = (*env)->NewString(
            env, ifRow->Description, (jsize) wcslen(ifRow->Description));
    if (displayName == NULL) {
        return NULL;
    }
    (*env)->SetObjectField(env, netifObj, ni_displayNameID, displayName);
    (*env)->DeleteLocalRef(env, displayName);

    // set the NetworkInterface's index
    (*env)->SetIntField(env, netifObj, ni_indexID, ifRow->InterfaceIndex);

    // find addresses associated with this interface
    for (i = 0; i < uniAddrs->NumEntries; i++) {
        if (uniAddrs->Table[i].InterfaceLuid.Value == ifRow->InterfaceLuid.Value &&
                (uniAddrs->Table[i].DadState == IpDadStatePreferred ||
                        uniAddrs->Table[i].DadState == IpDadStateDeprecated)) {
            addrCount++;
            addrsCurrent = malloc(sizeof(netaddr));
            if (addrsCurrent == NULL) {
                freeNetaddrs(addrsHead);
                JNU_ThrowOutOfMemoryError(env, "native heap");
                return NULL;
            }
            addrsCurrent->Address = uniAddrs->Table[i].Address;
            addrsCurrent->PrefixLength = uniAddrs->Table[i].OnLinkPrefixLength;
            addrsCurrent->Next = addrsHead;
            addrsHead = addrsCurrent;
        }
    }
    for (i = 0; i < anyAddrs->NumEntries; i++) {
        if (anyAddrs->Table[i].InterfaceLuid.Value == ifRow->InterfaceLuid.Value) {
            addrCount++;
            addrsCurrent = malloc(sizeof(netaddr));
            if (addrsCurrent == NULL) {
                freeNetaddrs(addrsHead);
                JNU_ThrowOutOfMemoryError(env, "native heap");
                return NULL;
            }
            addrsCurrent->Address = anyAddrs->Table[i].Address;
            addrsCurrent->PrefixLength = NO_PREFIX;
            addrsCurrent->Next = addrsHead;
            addrsHead = addrsCurrent;
        }
    }

    // instantiate the addrs and bindings array
    addrArr = (*env)->NewObjectArray(env, addrCount, ia_class, NULL);
    if (addrArr == NULL) {
        freeNetaddrs(addrsHead);
        return NULL;
    }
    bindsArr = (*env)->NewObjectArray(env, addrCount, ni_ibcls, NULL);
    if (bindsArr == NULL) {
        freeNetaddrs(addrsHead);
        return NULL;
    }

    // populate the addrs and bindings arrays
    i = 0;
    while (addrsCurrent != NULL) {
        if (addrsCurrent->Address.si_family == AF_INET) { // IPv4
            // create and populate InetAddress object
            inetAddr = (*env)->NewObject(env, ia4_class, ia4_ctrID);
            if (inetAddr == NULL) {
                freeNetaddrs(addrsHead);
                return NULL;
            }
            setInetAddress_addr(
                    env, inetAddr, ntohl(addrsCurrent->Address.Ipv4.sin_addr.s_addr));
            if ((*env)->ExceptionCheck(env)) {
                freeNetaddrs(addrsHead);
                return NULL;
            }

            // create and populate InterfaceAddress object
            bindAddr = (*env)->NewObject(env, ni_ibcls, ni_ibctrID);
            if (bindAddr == NULL) {
                freeNetaddrs(addrsHead);
                return NULL;
            }
            (*env)->SetObjectField(env, bindAddr, ni_ibaddressID, inetAddr);
            if (addrsCurrent->PrefixLength != NO_PREFIX) {
                (*env)->SetShortField(
                        env, bindAddr, ni_ibmaskID, addrsCurrent->PrefixLength);
                apiRetVal = CompatConvertLengthToIpv4Mask(addrsCurrent->PrefixLength, &mask);
                if (apiRetVal != NO_ERROR) {
                    freeNetaddrs(addrsHead);
                    SetLastError(apiRetVal);
                    NET_ThrowByNameWithLastError(
                            env, JNU_JAVANETPKG "SocketException",
                            "ConvertLengthToIpv4Mask");
                    return NULL;
                }
                bcastAddr = (*env)->NewObject(env, ia4_class, ia4_ctrID);
                if (bcastAddr == NULL) {
                    freeNetaddrs(addrsHead);
                    return NULL;
                }
                setInetAddress_addr(
                        env, bcastAddr,
                        ntohl(addrsCurrent->Address.Ipv4.sin_addr.s_addr | ~mask));
                if ((*env)->ExceptionCheck(env)) {
                    freeNetaddrs(addrsHead);
                    return NULL;
                }
                (*env)->SetObjectField(env, bindAddr, ni_ibbroadcastID, bcastAddr);
                (*env)->DeleteLocalRef(env, bcastAddr);
            }
        } else { // IPv6
            inetAddr = (*env)->NewObject(env, ia6_class, ia6_ctrID);
            if (inetAddr == NULL) {
                freeNetaddrs(addrsHead);
                return NULL;
            }
            if (setInet6Address_ipaddress(
                    env, inetAddr,
                    (jbyte *)&(addrsCurrent->Address.Ipv6.sin6_addr.s6_addr))
                    == JNI_FALSE) {
                freeNetaddrs(addrsHead);
                return NULL;
            }
            /* zero is default value, no need to set */
            if (addrsCurrent->Address.Ipv6.sin6_scope_id != 0) {
                setInet6Address_scopeid(
                        env, inetAddr, addrsCurrent->Address.Ipv6.sin6_scope_id);
                setInet6Address_scopeifname(env, inetAddr, netifObj);
            }
            bindAddr = (*env)->NewObject(env, ni_ibcls, ni_ibctrID);
            if (bindAddr == NULL) {
                freeNetaddrs(addrsHead);
                return NULL;
            }
            (*env)->SetObjectField(env, bindAddr, ni_ibaddressID, inetAddr);
            if (addrsCurrent->PrefixLength != NO_PREFIX) {
                (*env)->SetShortField(
                        env, bindAddr, ni_ibmaskID, addrsCurrent->PrefixLength);
            }
        }

        // add the new elements to the arrays
        (*env)->SetObjectArrayElement(env, addrArr, i, inetAddr);
        (*env)->DeleteLocalRef(env, inetAddr);
        (*env)->SetObjectArrayElement(env, bindsArr, i, bindAddr);
        (*env)->DeleteLocalRef(env, bindAddr);

        // advance to the next address
        addrsCurrent = addrsCurrent->Next;
        i++;
    }

    // free the address list since we no longer need it
    freeNetaddrs(addrsHead);

    // set the addrs and bindings arrays on the NetworkInterface
    (*env)->SetObjectField(env, netifObj, ni_addrsID, addrArr);
    (*env)->DeleteLocalRef(env, addrArr);
    (*env)->SetObjectField(env, netifObj, ni_bindsID, bindsArr);
    (*env)->DeleteLocalRef(env, bindsArr);

    // set child array on the NetworkInterface
    // Windows doesn't have virtual interfaces, so this is always empty
    childArr = (*env)->NewObjectArray(env, 0, ni_class, NULL);
    if (childArr == NULL) {
        return NULL;
    }
    (*env)->SetObjectField(env, netifObj, ni_childsID, childArr);
    (*env)->DeleteLocalRef(env, childArr);

    return netifObj;
}

/*
 * Builds and returns a java.net.NetworkInterface object from the given MIB_IF_ROW2.
 * This expects that the row is not yet populated, but an index has been set,
 * so the row is ready to be populated by GetIfEntry2.
 * If anything goes wrong, an exception will be set,
 * but the address tables are not freed.
 * Freeing the address tables is always the caller's responsibility.
 */
static jobject createNetworkInterfaceForSingleRowWithTables(
        JNIEnv *env, MIB_IF_ROW2 *ifRow,
        MIB_UNICASTIPADDRESS_TABLE *uniAddrs, MIB_ANYCASTIPADDRESS_TABLE *anyAddrs) {
    ULONG apiRetVal;

    apiRetVal = CompatGetIfEntry2(ifRow);
    if (apiRetVal != NO_ERROR) {
        if (apiRetVal != ERROR_FILE_NOT_FOUND) {
            SetLastError(apiRetVal);
            NET_ThrowByNameWithLastError(
                    env, JNU_JAVANETPKG "SocketException", "GetIfEntry2");
        }
        return NULL;
    }
    return createNetworkInterface(env, ifRow, uniAddrs, anyAddrs);
}

/*
 * Builds and returns a java.net.NetworkInterface object from the given MIB_IF_ROW2.
 * This expects that the row is not yet populated, but an index has been set,
 * so the row is ready to be populated by GetIfEntry2.
 * Unlike createNetworkInterfaceForSingleRowWithTables, this will get the address
 * tables at the beginning and free them at the end.
 * If anything goes wrong, an exception will be set.
 */
static jobject createNetworkInterfaceForSingleRow(
        JNIEnv *env, MIB_IF_ROW2 *ifRow) {
    MIB_UNICASTIPADDRESS_TABLE *uniAddrs;
    MIB_ANYCASTIPADDRESS_TABLE *anyAddrs;
    jobject netifObj;

    if (getAddressTables(env, &uniAddrs, &anyAddrs) == FALSE) {
        return NULL;
    }

    netifObj = createNetworkInterfaceForSingleRowWithTables(
            env, ifRow, uniAddrs, anyAddrs);

    CompatFreeMibTable(uniAddrs);
    CompatFreeMibTable(anyAddrs);

    return netifObj;
}

/*
 * Class:     NetworkInterface
 * Method:    getByIndex0
 * Signature: (I)LNetworkInterface;
 */
JNIEXPORT jobject JNICALL Java_java_net_NetworkInterface_getByIndex0(
        JNIEnv *env, jclass cls, jint index) {
    MIB_IF_ROW2 ifRow = {0};

    if (index == 0) {
        // 0 is never a valid index, and would make GetIfEntry2 think nothing is set
        return NULL;
    }

    ifRow.InterfaceIndex = index;
    return createNetworkInterfaceForSingleRow(env, &ifRow);
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    getByName0
 * Signature: (Ljava/lang/String;)Ljava/net/NetworkInterface;
 */
JNIEXPORT jobject JNICALL Java_java_net_NetworkInterface_getByName0(
        JNIEnv *env, jclass cls, jstring name) {
    const jchar *nameChars;
    ULONG apiRetVal;
    MIB_IF_ROW2 ifRow = {0};

    nameChars = (*env)->GetStringChars(env, name, NULL);
    apiRetVal = CompatConvertInterfaceNameToLuidW(nameChars, &(ifRow.InterfaceLuid));
    (*env)->ReleaseStringChars(env, name, nameChars);
    if (apiRetVal != ERROR_SUCCESS) {
        if (apiRetVal != ERROR_INVALID_NAME) {
            SetLastError(apiRetVal);
            NET_ThrowByNameWithLastError(
                    env, JNU_JAVANETPKG "SocketException",
                    "ConvertInterfaceNameToLuidW");
        }
        return NULL;
    }
    return createNetworkInterfaceForSingleRow(env, &ifRow);
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    getByInetAddress0
 * Signature: (Ljava/net/InetAddress;)Ljava/net/NetworkInterface;
 */
JNIEXPORT jobject JNICALL Java_java_net_NetworkInterface_getByInetAddress0(
        JNIEnv *env, jclass cls, jobject inetAddr) {
    MIB_UNICASTIPADDRESS_TABLE *uniAddrs;
    MIB_ANYCASTIPADDRESS_TABLE *anyAddrs;
    ULONG i;
    MIB_IF_ROW2 ifRow = {0};
    jobject result = NULL;

    if (getAddressTables(env, &uniAddrs, &anyAddrs) == FALSE) {
        return NULL;
    }

    for (i = 0; i < uniAddrs->NumEntries; i++) {
        if (NET_SockaddrEqualsInetAddress(
                env, (SOCKETADDRESS*) &(uniAddrs->Table[i].Address), inetAddr) &&
                (uniAddrs->Table[i].DadState == IpDadStatePreferred ||
                        uniAddrs->Table[i].DadState == IpDadStateDeprecated)) {
            ifRow.InterfaceLuid = uniAddrs->Table[i].InterfaceLuid;
            result = createNetworkInterfaceForSingleRowWithTables(
                    env, &ifRow, uniAddrs, anyAddrs);
            goto done;
        }
    }
    for (i = 0; i < anyAddrs->NumEntries; i++) {
        if (NET_SockaddrEqualsInetAddress(
                env, (SOCKETADDRESS*) &(anyAddrs->Table[i].Address), inetAddr)) {
            ifRow.InterfaceLuid = anyAddrs->Table[i].InterfaceLuid;
            result = createNetworkInterfaceForSingleRowWithTables(
                    env, &ifRow, uniAddrs, anyAddrs);
            goto done;
        }
    }

    done:
    CompatFreeMibTable(uniAddrs);
    CompatFreeMibTable(anyAddrs);
    return result;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    boundInetAddress0
 * Signature: (Ljava/net/InetAddress;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_boundInetAddress0(
        JNIEnv *env, jclass cls, jobject inetAddr) {
    MIB_UNICASTIPADDRESS_TABLE *uniAddrs;
    MIB_ANYCASTIPADDRESS_TABLE *anyAddrs;
    ULONG i;
    jboolean result = JNI_FALSE;

    if (getAddressTables(env, &uniAddrs, &anyAddrs) == FALSE) {
        return JNI_FALSE;
    }

    for (i = 0; i < uniAddrs->NumEntries; i++) {
        if (NET_SockaddrEqualsInetAddress(
                env, (SOCKETADDRESS*) &(uniAddrs->Table[i].Address), inetAddr) &&
                (uniAddrs->Table[i].DadState == IpDadStatePreferred ||
                        uniAddrs->Table[i].DadState == IpDadStateDeprecated)) {
            result = JNI_TRUE;
            goto done;
        }
    }
    for (i = 0; i < anyAddrs->NumEntries; i++) {
        if (NET_SockaddrEqualsInetAddress(
                env, (SOCKETADDRESS*) &(anyAddrs->Table[i].Address), inetAddr)) {
            result = JNI_TRUE;
            goto done;
        }
    }

    done:
    CompatFreeMibTable(uniAddrs);
    CompatFreeMibTable(anyAddrs);
    return result;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    getAll
 * Signature: ()[Ljava/net/NetworkInterface;
 */
JNIEXPORT jobjectArray JNICALL Java_java_net_NetworkInterface_getAll(
        JNIEnv *env, jclass cls) {
    MIB_IF_TABLE2 *ifTable;
    jobjectArray ifArray;
    MIB_UNICASTIPADDRESS_TABLE *uniAddrs;
    MIB_ANYCASTIPADDRESS_TABLE *anyAddrs;
    ULONG apiRetVal, i;
    jobject ifObj;

    apiRetVal = CompatGetIfTable2(&ifTable);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetIfTable2");
        return NULL;
    }

    ifArray = (*env)->NewObjectArray(env, ifTable->NumEntries, cls, NULL);
    if (ifArray == NULL) {
        CompatFreeMibTable(ifTable);
        return NULL;
    }

    if (getAddressTables(env, &uniAddrs, &anyAddrs) == FALSE) {
        CompatFreeMibTable(ifTable);
        return NULL;
    }

    for (i = 0; i < ifTable->NumEntries; i++) {
        ifObj = createNetworkInterface(
                env, &(ifTable->Table[i]), uniAddrs, anyAddrs);
        if (ifObj == NULL) {
            CompatFreeMibTable(ifTable);
            CompatFreeMibTable(uniAddrs);
            CompatFreeMibTable(anyAddrs);
            return NULL;
        }
        (*env)->SetObjectArrayElement(env, ifArray, i, ifObj);
        (*env)->DeleteLocalRef(env, ifObj);
    }

    CompatFreeMibTable(ifTable);
    CompatFreeMibTable(uniAddrs);
    CompatFreeMibTable(anyAddrs);
    return ifArray;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    isUp0
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_isUp0(
        JNIEnv *env, jclass cls, jstring name, jint index) {
    MIB_IF_ROW2 ifRow = {0};
    ULONG apiRetVal;

    ifRow.InterfaceIndex = index;
    apiRetVal = CompatGetIfEntry2(&ifRow);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetIfEntry2");
        return JNI_FALSE;
    }
    return ifRow.AdminStatus == NET_IF_ADMIN_STATUS_UP &&
            ifRow.OperStatus == IfOperStatusUp
            ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    isP2P0
 * Signature: (Ljava/lang/String;I)Z
 */
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_isP2P0(
        JNIEnv *env, jclass cls, jstring name, jint index) {
    MIB_IF_ROW2 ifRow = {0};
    ULONG apiRetVal;

    ifRow.InterfaceIndex = index;
    apiRetVal = CompatGetIfEntry2(&ifRow);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetIfEntry2");
        return JNI_FALSE;
    }
    return ifRow.AccessType == NET_IF_ACCESS_POINT_TO_POINT ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    isLoopback0
 * Signature: (Ljava/lang/String;I)Z
 */
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_isLoopback0(
        JNIEnv *env, jclass cls, jstring name, jint index) {
    MIB_IF_ROW2 ifRow = {0};
    ULONG apiRetVal;

    ifRow.InterfaceIndex = index;
    apiRetVal = CompatGetIfEntry2(&ifRow);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetIfEntry2");
        return JNI_FALSE;
    }
    return ifRow.Type == IF_TYPE_SOFTWARE_LOOPBACK ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    getMacAddr0
 * Signature: ([bLjava/lang/String;I)[b
 */
JNIEXPORT jbyteArray JNICALL Java_java_net_NetworkInterface_getMacAddr0(
        JNIEnv *env, jclass class, jbyteArray addrArray, jstring name, jint index) {
    MIB_IF_ROW2 ifRow = {0};
    ULONG apiRetVal;
    jbyteArray macAddr;

    ifRow.InterfaceIndex = index;
    apiRetVal = CompatGetIfEntry2(&ifRow);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetIfEntry2");
        return NULL;
    }
    if (ifRow.PhysicalAddressLength == 0) {
        return NULL;
    }
    macAddr = (*env)->NewByteArray(env, ifRow.PhysicalAddressLength);
    if (macAddr == NULL) {
        return NULL;
    }
    (*env)->SetByteArrayRegion(
            env, macAddr, 0, ifRow.PhysicalAddressLength,
            (jbyte *) ifRow.PhysicalAddress);
    return macAddr;
}

/*
 * Class:       java_net_NetworkInterface
 * Method:      getMTU0
 * Signature:   ([bLjava/lang/String;I)I
 */
JNIEXPORT jint JNICALL Java_java_net_NetworkInterface_getMTU0(
        JNIEnv *env, jclass class, jstring name, jint index) {
    MIB_IF_ROW2 ifRow = {0};
    ULONG apiRetVal;

    ifRow.InterfaceIndex = index;
    apiRetVal = CompatGetIfEntry2(&ifRow);
    if (apiRetVal != NO_ERROR) {
        SetLastError(apiRetVal);
        NET_ThrowByNameWithLastError(
                env, JNU_JAVANETPKG "SocketException", "GetIfEntry2");
        return -1;
    }
    return ifRow.Mtu;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    supportsMulticast0
 * Signature: (Ljava/lang/String;I)Z
 */
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_supportsMulticast0(
        JNIEnv *env, jclass cls, jstring name, jint index) {
    // we assume that multicast is enabled, because there are no reliable APIs to tell us
    return JNI_TRUE;
}

/*
 * Class:     java_net_NetworkInterface
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_net_NetworkInterface_init(JNIEnv *env, jclass cls) {
    /*
     * Get the various JNI ids that we require
     */
    ni_class = (*env)->NewGlobalRef(env, cls);
    CHECK_NULL(ni_class);
    ni_nameID = (*env)->GetFieldID(env, ni_class, "name", "Ljava/lang/String;");
    CHECK_NULL(ni_nameID);
    ni_displayNameID = (*env)->GetFieldID(env, ni_class, "displayName", "Ljava/lang/String;");
    CHECK_NULL(ni_displayNameID);
    ni_indexID = (*env)->GetFieldID(env, ni_class, "index", "I");
    CHECK_NULL(ni_indexID);
    ni_addrsID = (*env)->GetFieldID(env, ni_class, "addrs", "[Ljava/net/InetAddress;");
    CHECK_NULL(ni_addrsID);
    ni_bindsID = (*env)->GetFieldID(env, ni_class, "bindings", "[Ljava/net/InterfaceAddress;");
    CHECK_NULL(ni_bindsID);
    ni_childsID = (*env)->GetFieldID(env, ni_class, "childs", "[Ljava/net/NetworkInterface;");
    CHECK_NULL(ni_childsID);
    ni_ctor = (*env)->GetMethodID(env, ni_class, "<init>", "()V");
    CHECK_NULL(ni_ctor);
    ni_ibcls = (*env)->FindClass(env, "java/net/InterfaceAddress");
    CHECK_NULL(ni_ibcls);
    ni_ibcls = (*env)->NewGlobalRef(env, ni_ibcls);
    CHECK_NULL(ni_ibcls);
    ni_ibctrID = (*env)->GetMethodID(env, ni_ibcls, "<init>", "()V");
    CHECK_NULL(ni_ibctrID);
    ni_ibaddressID = (*env)->GetFieldID(env, ni_ibcls, "address", "Ljava/net/InetAddress;");
    CHECK_NULL(ni_ibaddressID);
    ni_ibbroadcastID = (*env)->GetFieldID(env, ni_ibcls, "broadcast", "Ljava/net/Inet4Address;");
    CHECK_NULL(ni_ibbroadcastID);
    ni_ibmaskID = (*env)->GetFieldID(env, ni_ibcls, "maskLength", "S");
    CHECK_NULL(ni_ibmaskID);

    initInetAddressIDs(env);
}
