#ifndef __CMD_PRECOMP_H
#define __CMD_PRECOMP_H

// Include our compatibility layer first
#include "compat.h"
#include "ndk_compat.h"

// Standard C headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>

// tchar.h is not available in this environment.
// We are compiling for UNICODE, so we will rely on wchar.h and windows.h
// #include <tchar.h>

// All necessary headers are now included via compat.h and ndk_compat.h

// Include shell API for functions like ShellExecute
#include <shellapi.h>

// String safe functions
#include <strsafe.h>

// Include our compatibility layer for ReactOS conutils
#include "conutils_compat.h"

// Project-specific headers
#include "resource.h"
#include "cmd.h"
#include "config.h"
#include "batch.h"

// The original file had Wine debugging macros.
// We are removing them for a standalone build.
// If debug output is needed, use DbgPrint which maps to printf.
#ifdef UNICODE
#define debugstr_aw(str)
#else
#define debugstr_aw(str)
#endif

#endif /* __CMD_PRECOMP_H */
