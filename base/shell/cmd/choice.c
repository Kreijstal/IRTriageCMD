/*
 *  CHOICE - choice internal command
 *
 *  History
 *
 *    16-Nov-2004 (Created by ???)
 *
 *    02-Apr-2005 (Magnus Olsen <magnus@greatlord.com>)
 *        Remove all hardcoded strings in En.rc
 *
 */

#include "precomp.h"

#ifdef INCLUDE_CMD_CHOICE

#define IS_SWITCH(c) ((c) == '/' || (c) == '-')
#define TIME_OUT_EL 255

static TCHAR get_choice(LPTSTR lpAllowed, BOOL bCase)
{
    TCHAR  str[2];
    TCHAR *p;
    DWORD read;

    str[1] = 0;
    for (;;)
    {
        if (ReadConsole(GetStdHandle(STD_INPUT_HANDLE), str, 1, &read, NULL) && read)
        {
            for (p = lpAllowed; *p; p++)
            {
                if (bCase)
                {
                    if (*p == *str)
                        return *p;
                }
                else
                {
                    if (_totupper(*p) == _totupper(*str))
                        return *str;
                }
            }
        }
    }
}


INT CommandChoice(LPTSTR param)
{
    LPTSTR  *arg;
    INT     argc, i;
    TCHAR   *s, *lpText;
    TCHAR   *lpAllowed = _T("YN");
    TCHAR   *lpDefault = NULL;
    BOOL    bCase = FALSE;
    BOOL    bShowN = FALSE;
    BOOL    bTimeout = FALSE;
    UINT    nTimeout = 0;
    DWORD   start, left, dw;

    /* print help */
    if (param && !_tcsncmp(param, _T("/?"), 2))
    {
        wprintf(L"CHOICE [/C choices] [/N] [/S] [/T timeout /D choice] [text]\n\n"
                L"  /C[:]choices  Specifies allowable keys. Default is YN\n"
                L"  /N            Hides the key list in the prompt\n"
                L"  /S            Causes the choice to be case-sensitive\n"
                L"  /T[:]c,nn     Times out in nn seconds, default is c\n"
                L"  text          The prompt string\n");
        return 0;
    }

    nErrorLevel = 0;

    /* build parameter array */
    arg = split(param, &argc, TRUE, TRUE);

    for (i = 0; i < argc; i++)
    {
        if (IS_SWITCH(arg[i][0]))
        {
            s = arg[i] + 1;
            if (*(s - 1) == '-')
                s[-1] = '/';
            if (s[1] == ':')
                s++;
            switch (_totupper(*s++))
            {
                case 'C':
                    if (*s)
                        lpAllowed = s;
                    else
                    {
                        fwprintf(stderr, L"Invalid option. Expected format: /C[:]options\n");
                        return 1;
                    }
                    break;
                case 'N':
                    bShowN = TRUE;
                    break;
                case 'S':
                    bCase = TRUE;
                    break;
                case 'T':
                    if (*s)
                    {
                        lpDefault = s++;
                        if (*s == ',')
                            s++;
                        else
                        {
                            fwprintf(stderr, L"Invalid option. Expected format: /T[:]c,nn\n");
                            return 1;
                        }
                        if (*s)
                        {
                            bTimeout = TRUE;
                            nTimeout = _ttoi(s);
                        }
                    }
                    break;
                default:
                    fwprintf(stderr, L"Illegal Option: %s\n", arg[i]);
                    return 1;
            }
        }
    }

    if (argc > i)
    {
        lpText = arg[i];
    }
    else
        lpText = _T("");

    wprintf(L"%s", lpText);
    if (!bShowN)
    {
        wprintf(L" [%s]?", lpAllowed);
    }

    if (!bTimeout)
    {
        nErrorLevel = _tcschr(lpAllowed, get_choice(lpAllowed, bCase)) - lpAllowed + 1;
    }
    else
    {
        start = GetTickCount();
        for (;;)
        {
            left = nTimeout * 1000 - (GetTickCount() - start);
            if (left <= 0)
                break;
            if (WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), 100) == WAIT_OBJECT_0)
            {
                dw = 0;
                GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &dw);
                if (dw > 1)
                {
                    nErrorLevel = _tcschr(lpAllowed, get_choice(lpAllowed, bCase)) - lpAllowed + 1;
                    goto end;
                }
            }
        }
        nErrorLevel = TIME_OUT_EL;
        wprintf(L"%c", *lpDefault);
    }
  end:
    wprintf(L"\n");
    return nErrorLevel;
}

#endif /* INCLUDE_CMD_CHOICE */
