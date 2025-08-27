#ifndef CMD_COMPAT_H
#define CMD_COMPAT_H

#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <wctype.h>
#include <wchar.h>

// ReactOS/WDK DbgPrint -> standard printf
#define DbgPrint printf

// ReactOS/WDK ASSERT -> standard assert
#ifndef ASSERT
#define ASSERT assert
#endif

// tchar.h compatibility macros for UNICODE build
#define _T(x)       L##x
#define _istspace   iswspace
#define _totlower   towlower
#define _tcscspn    wcscspn
#define _tcsspn     wcsspn
#define _tcschr     wcschr
#define _tcsncmp    wcsncmp
#define _tcslen     wcslen
#define _tcsstr     wcsstr
#define _tcsicmp    wcsicmp
#define _tcsnicmp   wcsnicmp
#define _tcscpy     wcscpy
#define _tcscpy_s(dst, size, src) wcscpy(dst, src)
#define _tcsncpy    wcsncpy
#define _tcscat     wcscat
#define _stprintf   swprintf
#define _sntprintf  _snwprintf
#define _vstprintf  vswprintf
#define _ttoi       _wtoi
#define _istalpha   iswalpha
#define _tcsncat    wcsncat
#define _itot       _itow
#define _tcstol     wcstol
#define _ttol       _wtol
#define _tcspbrk    wcspbrk
#define _tchdir     _wchdir
#define _tcstoul    wcstoul
#define _tcsnset    _wcsnset
#define _TCHAR      WCHAR
#define TCHAR       WCHAR
#define LPCTSTR     LPCWSTR
#define LPTSTR      LPWSTR

// Dummy version strings from ReactOS buildno.h
#define KERNEL_VERSION_STR "0.4.14-dev"
#define KERNEL_VERSION_BUILD_STR "standalone"
#define COPYRIGHT_YEAR "2024"

// Disable Wine/ReactOS debugging macros
#define TRACE(...)
#define FIXME(...)
#define WARN(...)
#define ERR(...)

// List manipulation functions (from ntdef.h, but defined here to be safe)
static __inline void InitializeListHead(LIST_ENTRY* list) {
    list->Flink = list->Blink = list;
}
static __inline void InsertTailList(LIST_ENTRY* list, LIST_ENTRY* entry) {
    entry->Blink = list->Blink;
    entry->Flink = list;
    list->Blink->Flink = entry;
    list->Blink = entry;
}
static __inline void RemoveEntryList(LIST_ENTRY* entry) {
    entry->Blink->Flink = entry->Flink;
    entry->Flink->Blink = entry->Blink;
}

#endif // CMD_COMPAT_H
