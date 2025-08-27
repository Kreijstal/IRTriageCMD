#ifndef NDK_COMPAT_H
#define NDK_COMPAT_H

#include <windows.h>

// This file provides minimal definitions for NDK/internal structures
// that are not available in the standard public Windows headers, but are
// used by this application. The definitions are based on public
// documentation and reverse engineering resources.

// From winternl.h or ntddk.h
typedef LONG KPRIORITY;

// Minimal PEB definition to get ImageSubsystem
// Based on information from Geoff Chappell's website (www.geoffchappell.com)
// The offset for ImageSubsystem on x86 is 0xB4.
typedef struct _PEB {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    PVOID Reserved3[2];
    PVOID Ldr;
    PVOID ProcessParameters;
    PVOID SubSystemData;
    PVOID ProcessHeap;
    PVOID FastPebLock;
    PVOID Reserved4[3];
    PVOID Reserved5[2];
    PVOID FreeList;
    ULONG TlsExpansionCounter;
    PVOID TlsBitmap;
    ULONG TlsBitmapBits[2];
    PVOID ReadOnlySharedMemoryBase;
    PVOID ReadOnlySharedMemoryHeap;
    PVOID* ReadOnlyStaticServerData;
    PVOID AnsiCodePageData;
    PVOID OemCodePageData;
    PVOID UnicodeCaseTableData;
    ULONG NumberOfProcessors;
    ULONG NtGlobalFlag;
    LARGE_INTEGER CriticalSectionTimeout;
    ULONG_PTR HeapSegmentReserve;
    ULONG_PTR HeapSegmentCommit;
    ULONG_PTR HeapDeCommitTotalFreeThreshold;
    ULONG_PTR HeapDeCommitFreeBlockThreshold;
    ULONG NumberOfHeaps;
    ULONG MaximumNumberOfHeaps;
    PVOID* ProcessHeaps;
    PVOID GdiSharedHandleTable;
    PVOID ProcessStarterHelper;
    ULONG GdiDCAttributeList;
    PVOID LoaderLock;
    ULONG OSMajorVersion;
    ULONG OSMinorVersion;
    USHORT OSBuildNumber;
    USHORT OSCSDVersion;
    ULONG OSPlatformId;
    ULONG ImageSubsystem; // This is what we need at offset 0xB4
    // Other fields follow, but we don't need them for this build.
} PEB, *PPEB;


// From winternl.h
typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PPEB PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;


// From winternl.h
typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    // Other values are not needed for this build.
} PROCESSINFOCLASS;


// Function pointer type used in cmd.c
typedef NTSTATUS (WINAPI *NtQueryInformationProcessProc)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
typedef NTSTATUS (WINAPI *NtReadVirtualMemoryProc)(HANDLE, PVOID, PVOID, ULONG, PULONG);


#endif // NDK_COMPAT_H
