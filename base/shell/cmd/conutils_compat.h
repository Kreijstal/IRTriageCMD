#ifndef CONUTILS_COMPAT_H
#define CONUTILS_COMPAT_H

#include <windows.h>

// Explicitly define TCHAR types to solve ordering issues
// and dependency on tchar.h
typedef WCHAR TCHAR;
typedef LPWSTR PTCH, LPTSTR;
typedef LPCWSTR PCTCH, LPCTSTR;

// Forward declarations for circular dependencies
struct _CON_STREAM;
struct _CON_PAGER;

// from stream.h
typedef enum _CON_STREAM_MODE
{
    Binary = 0,
    AnsiText,
    WideText,
    UTF16Text,
    UTF8Text,
} CON_STREAM_MODE, *PCON_STREAM_MODE;

// from stream.h / outstream.h
typedef INT (__stdcall *CON_WRITE_FUNC)(
    IN struct _CON_STREAM* Stream,
    IN PCTCH szStr,
    IN DWORD len);

// from stream_private.h
typedef struct _CON_STREAM
{
    CON_WRITE_FUNC WriteFunc;
    BOOL IsInitialized;
    CRITICAL_SECTION Lock;
    HANDLE hHandle;
    BOOL IsConsole;
    CON_STREAM_MODE Mode;
    UINT CodePage;
} CON_STREAM, *PCON_STREAM;

// from screen.h
typedef struct _CON_SCREEN
{
    PCON_STREAM Stream;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    CONSOLE_CURSOR_INFO cci;
} CON_SCREEN, *PCON_SCREEN;

// from pager.h
typedef BOOL (__stdcall *CON_PAGER_LINE_FN)(
    IN OUT struct _CON_PAGER *Pager,
    IN PCTCH line,
    IN DWORD cch);

// from pager.h
typedef struct _CON_PAGER
{
    PCON_SCREEN Screen;
    DWORD PageColumns;
    DWORD PageRows;
    CON_PAGER_LINE_FN PagerLine;
    DWORD dwFlags;
    LONG nTabWidth;
    DWORD ScrollRows;
    PCTCH CachedLine;
    SIZE_T cchCachedLine;
    SIZE_T ich;
    PCTCH CurrentLine;
    SIZE_T ichCurr;
    SIZE_T iEndLine;
    DWORD nSpacePending;
    DWORD iColumn;
    DWORD iLine;
    DWORD lineno;
} CON_PAGER, *PCON_PAGER;

#endif // CONUTILS_COMPAT_H
