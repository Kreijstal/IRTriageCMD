/*
 *  CLS - cls internal command
 *
 *  History
 *
 *    19-Jan-1999 (Eric Kohl)
 *        started
 *
 */

#include "precomp.h"

#ifdef INCLUDE_CMD_CLS

static void ClearScreen(void)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD cCharsWritten;
    DWORD dwConSize;
    COORD coordScreen = { 0, 0 };

    /* Get the number of character cells in the current buffer */
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
        return;

    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* Fill the entire screen with blanks */
    if (!FillConsoleOutputCharacter(hConsole,
                                    (TCHAR)' ',
                                    dwConSize,
                                    coordScreen,
                                    &cCharsWritten))
        return;

    /* Get the current text attribute */
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
        return;

    /* Set the buffer's attributes accordingly */
    if (!FillConsoleOutputAttribute(hConsole,
                                    csbi.wAttributes,
                                    dwConSize,
                                    coordScreen,
                                    &cCharsWritten))
        return;

    /* Put the cursor at its home coordinates */
    SetConsoleCursorPosition(hConsole, coordScreen);
}


INT cmd_cls(LPTSTR param)
{
    ClearScreen();
    return 0;
}

#endif /* INCLUDE_CMD_CLS */
