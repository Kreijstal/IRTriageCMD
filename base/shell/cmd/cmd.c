/*
 *  CMD.C - command-line interface.
 */

#include "precomp.h"

#ifndef NT_SUCCESS
#define NT_SUCCESS(StatCode)  ((NTSTATUS)(StatCode) >= 0)
#endif

BOOL bExit = FALSE;       /* indicates EXIT was typed */
BOOL bCanExit = TRUE;     /* indicates if this shell is exitable */
BOOL bCtrlBreak = FALSE;  /* Ctrl-Break or Ctrl-C hit */
BOOL bIgnoreEcho = FALSE; /* Set this to TRUE to prevent a newline, when executing a command */
static BOOL bWaitForCommand = FALSE; /* When we are executing something passed on the commandline after /c or /k */
INT  nErrorLevel = 0;     /* Errorlevel of last launched external program */
CRITICAL_SECTION ChildProcessRunningLock;
BOOL bUnicodeOutput = FALSE;
BOOL bDisableBatchEcho = FALSE;
BOOL bEnableExtensions = TRUE;
BOOL bDelayedExpansion = FALSE;
BOOL bTitleSet = FALSE;
DWORD dwChildProcessId = 0;
OSVERSIONINFO osvi;
HANDLE hIn;
HANDLE hOut;
LPTSTR lpOriginalEnvironment;
HANDLE CMD_ModuleHandle;

static NtQueryInformationProcessProc NtQueryInformationProcessPtr = NULL;
static NtReadVirtualMemoryProc       NtReadVirtualMemoryPtr = NULL;

#ifdef INCLUDE_CMD_COLOR
WORD wDefColor = 0;     /* Default color */
#endif

/*
 * convert
 *
 * insert commas into a number
 */
INT
ConvertULargeInteger(ULONGLONG num, LPTSTR des, UINT len, BOOL bPutSeperator)
{
	TCHAR temp[39];   /* maximum length with nNumberGroups == 1 */
	UINT  n, iTarget;

	if (len <= 1)
		return 0;

	n = 0;
	iTarget = nNumberGroups;
	if (!nNumberGroups)
		bPutSeperator = FALSE;

	do
	{
		if (iTarget == n && bPutSeperator)
		{
			iTarget += nNumberGroups + 1;
			temp[38 - n++] = cThousandSeparator;
		}
		temp[38 - n++] = (TCHAR)(num % 10) + _T('0');
		num /= 10;
	} while (num > 0);
	if (n > len-1)
		n = len-1;

	memcpy(des, temp + 39 - n, n * sizeof(TCHAR));
	des[n] = _T('\0');

	return n;
}

/*
 * Is a process a console process?
 */
static BOOL IsConsoleProcess(HANDLE Process)
{
	NTSTATUS Status;
	PROCESS_BASIC_INFORMATION Info;
	PEB ProcessPeb;
	ULONG BytesRead;

	if (NULL == NtQueryInformationProcessPtr || NULL == NtReadVirtualMemoryPtr)
	{
		return TRUE;
	}

	Status = NtQueryInformationProcessPtr (
		Process, ProcessBasicInformation,
		&Info, sizeof(PROCESS_BASIC_INFORMATION), NULL);
	if (! NT_SUCCESS(Status))
	{
		WARN ("NtQueryInformationProcess failed with status %08x\n", Status);
		return TRUE;
	}
	Status = NtReadVirtualMemoryPtr (
		Process, Info.PebBaseAddress, &ProcessPeb,
		sizeof(PEB), &BytesRead);
	if (! NT_SUCCESS(Status) || sizeof(PEB) != BytesRead)
	{
		WARN ("Couldn't read virt mem status %08x bytes read %lu\n", Status, BytesRead);
		return TRUE;
	}

	return IMAGE_SUBSYSTEM_WINDOWS_CUI == ProcessPeb.ImageSubsystem;
}



#ifdef _UNICODE
#define SHELLEXECUTETEXT   	"ShellExecuteExW"
#else
#define SHELLEXECUTETEXT   	"ShellExecuteExA"
#endif

typedef BOOL (WINAPI *MYEX)(LPSHELLEXECUTEINFO lpExecInfo);

HANDLE RunFile(DWORD flags, LPTSTR filename, LPTSTR params,
               LPTSTR directory, INT show)
{
	SHELLEXECUTEINFO sei;
	HMODULE     hShell32;
	MYEX        hShExt;
	BOOL        ret;

	TRACE ("RunFile(%s)\n", filename);
	hShell32 = LoadLibrary(_T("SHELL32.DLL"));
	if (!hShell32)
	{
		WARN ("RunFile: couldn't load SHELL32.DLL!\n");
		return NULL;
	}

	hShExt = (MYEX)(FARPROC)GetProcAddress(hShell32, SHELLEXECUTETEXT);
	if (!hShExt)
	{
		WARN ("RunFile: couldn't find ShellExecuteExA/W in SHELL32.DLL!\n");
		FreeLibrary(hShell32);
		return NULL;
	}

	TRACE ("RunFile: ShellExecuteExA/W is at %x\n", hShExt);

	memset(&sei, 0, sizeof sei);
	sei.cbSize = sizeof sei;
	sei.fMask = flags;
	sei.lpFile = filename;
	sei.lpParameters = params;
	sei.lpDirectory = directory;
	sei.nShow = show;
	ret = hShExt(&sei);

	TRACE ("RunFile: ShellExecuteExA/W returned 0x%p\n", ret);

	FreeLibrary(hShell32);
	return ret ? sei.hProcess : NULL;
}



/*
 * This command (in first) was not found in the command table
 *
 * Full  - buffer to hold whole command line
 * First - first word on command line
 * Rest  - rest of command line
 */

static INT
Execute (LPTSTR Full, LPTSTR First, LPTSTR Rest, PARSED_COMMAND *Cmd)
{
    TCHAR szFullName[MAX_PATH];
    TCHAR *first, *rest, *dot;
    TCHAR szWindowTitle[MAX_PATH];
    TCHAR szNewTitle[MAX_PATH*2];
    DWORD dwExitCode = 0;
    TCHAR *FirstEnd;
    TCHAR szFullCmdLine[CMDLINE_LENGTH];

	TRACE ("Execute: \'%s\' \'%s\'\n", First, Rest);

	/* Though it was already parsed once, we have a different set of rules
	   for parsing before we pass to CreateProccess */
	if (First[0] == _T('/') || (First[0] && First[1] == _T(':')))
	{
		/* Use the entire first word as the program name (no change) */
		FirstEnd = First + _tcslen(First);
	}
	else
	{
		/* If present in the first word, spaces and ,;=/ end the program
		 * name and become the beginning of its parameters. */
		BOOL bInside = FALSE;
		for (FirstEnd = First; *FirstEnd; FirstEnd++)
		{
			if (!bInside && (_istspace(*FirstEnd) || _tcschr(_T(",;=/"), *FirstEnd)))
				break;
			bInside ^= *FirstEnd == _T('"');
		}
	}

	/* Copy the new first/rest into the buffer */
	first = Full;
	rest = &Full[FirstEnd - First + 1];
	StringCchCopyW(rest, CMDLINE_LENGTH - (rest - Full), FirstEnd);
	StringCchCatW(rest, CMDLINE_LENGTH - (rest - Full), Rest);
	*FirstEnd = _T('\0');
	StringCchCopyW(first, (FirstEnd - First) + 1, First);

	/* check for a drive change */
	if ((_istalpha (first[0])) && (!_tcscmp (first + 1, _T(":"))))
	{
		BOOL working = TRUE;
		if (!SetCurrentDirectory(first))
		/* Guess they changed disc or something, handle that gracefully and get to root */
		{
			TCHAR str[4];
			str[0]=first[0];
			str[1]=_T(':');
			str[2]=_T('\\');
			str[3]=0;
			working = SetCurrentDirectory(str);
		}

		if (!working) fwprintf(stderr, L"The system cannot find the drive specified.\n");
		return !working;
	}
	/* get the PATH environment variable and parse it */
	/* search the PATH environment variable for the binary */
	StripQuotes(First);
	if (!SearchForExecutable(First, szFullName))
	{
		error_bad_command(first);
		return 1;
	}

    /* Save the original console title and build a new one */
    GetConsoleTitle(szWindowTitle, ARRAYSIZE(szWindowTitle));
    bTitleSet = FALSE;
    StringCchPrintfW(szNewTitle, ARRAYSIZE(szNewTitle),
                    _T("%s - %s%s"), szWindowTitle, First, Rest);
    SetConsoleTitleW(szNewTitle);

	/* check if this is a .BAT or .CMD file */
	dot = _tcsrchr (szFullName, _T('.'));
	if (dot && (!_tcsicmp (dot, _T(".bat")) || !_tcsicmp (dot, _T(".cmd"))))
	{
		while (*rest == _T(' '))
			rest++;
		TRACE ("[BATCH: %s %s]\n", szFullName, rest);
		dwExitCode = Batch(szFullName, first, rest, Cmd);
	}
	else
	{
		/* exec the program */
		PROCESS_INFORMATION prci;
		STARTUPINFO stui;

        /* build command line for CreateProcess(): FullName + " " + rest */
        BOOL quoted = !!_tcschr(First, ' ');
        StringCchCopyW(szFullCmdLine, CMDLINE_LENGTH, quoted ? _T("\"") : _T(""));
        StringCchCatNW(szFullCmdLine, CMDLINE_LENGTH, First, CMDLINE_LENGTH - _tcslen(szFullCmdLine) - 1);
        StringCchCatNW(szFullCmdLine, CMDLINE_LENGTH, quoted ? _T("\"") : _T(""), CMDLINE_LENGTH - _tcslen(szFullCmdLine) - 1);

        if (*rest)
        {
            StringCchCatNW(szFullCmdLine, CMDLINE_LENGTH, _T(" "), CMDLINE_LENGTH - _tcslen(szFullCmdLine) - 1);
            StringCchCatNW(szFullCmdLine, CMDLINE_LENGTH, rest, CMDLINE_LENGTH - _tcslen(szFullCmdLine) - 1);
        }

		TRACE ("[EXEC: %s]\n", szFullCmdLine);

		/* fill startup info */
		memset (&stui, 0, sizeof (STARTUPINFO));
		stui.cb = sizeof (STARTUPINFO);
		stui.dwFlags = STARTF_USESHOWWINDOW;
		stui.wShowWindow = SW_SHOWDEFAULT;

		/* Set the console to standard mode */
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),
                       ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

		if (CreateProcess(szFullName,
		                   szFullCmdLine,
		                   NULL,
		                   NULL,
		                   TRUE,
		                   0,			/* CREATE_NEW_PROCESS_GROUP */
		                   NULL,
		                   NULL,
		                   &stui,
		                   &prci))
		{
			CloseHandle(prci.hThread);
		}
		else
		{
			// See if we can run this with ShellExecute() ie myfile.xls
			prci.hProcess = RunFile(SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE,
			                        szFullName,
			                        rest,
			                        NULL,
			                        SW_SHOWNORMAL);
		}

		if (prci.hProcess != NULL)
		{
			if (bc != NULL || bWaitForCommand || IsConsoleProcess(prci.hProcess))
            {
                /* when processing a batch file or starting console processes: execute synchronously */
                EnterCriticalSection(&ChildProcessRunningLock);
                dwChildProcessId = prci.dwProcessId;

                WaitForSingleObject(prci.hProcess, INFINITE);

                LeaveCriticalSection(&ChildProcessRunningLock);

                GetExitCodeProcess(prci.hProcess, &dwExitCode);
                nErrorLevel = (INT)dwExitCode;
            }
            CloseHandle(prci.hProcess);
		}
		else
		{
			TRACE ("[ShellExecute failed!: %s]\n", Full);
			error_bad_command(first);
			dwExitCode = 1;
		}

		/* Restore our default console mode */
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),
                       ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
                       ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    }

	/* Update our local codepage cache */
    {
        UINT uNewInputCodePage  = GetConsoleCP();
        UINT uNewOutputCodePage = GetConsoleOutputCP();

        if ((InputCodePage  != uNewInputCodePage) ||
            (OutputCodePage != uNewOutputCodePage))
        {
            /* Update the locale as well */
            InitLocale();
        }

        InputCodePage  = uNewInputCodePage;
        OutputCodePage = uNewOutputCodePage;
    }

    /* Restore the original console title */
    if (!bTitleSet)
        SetConsoleTitleW(szWindowTitle);

	return dwExitCode;
}


/*
 * look through the internal commands and determine whether or not this
 * command is one of them.  If it is, call the command.  If not, call
 * execute to run it as an external program.
 */
INT
DoCommand(LPTSTR first, LPTSTR rest, PARSED_COMMAND *Cmd)
{
	TCHAR *com;
	TCHAR *cp;
	LPTSTR param;   /* pointer to command's parameters */
	INT cl;
	LPCOMMAND cmdptr;
	BOOL nointernal = FALSE;
	INT ret;

	TRACE ("DoCommand: (\'%s\' \'%s\')\n", first, rest);

	/* full command line */
	com = cmd_alloc((_tcslen(first) + _tcslen(rest) + 2) * sizeof(TCHAR));
	if (com == NULL)
	{
		error_out_of_memory();
		return 1;
	}

	/* If present in the first word, these characters end the name of an
	 * internal command and become the beginning of its parameters. */
	cp = first + _tcscspn(first, _T("\t +,/;=[]"));

	for (cl = 0; cl < (cp - first); cl++)
	{
		/* These characters do it too, but if one of them is present,
		 * then we check to see if the word is a file name and skip
		 * checking for internal commands if so.
		 * This allows running programs with names like "echo.exe" */
		if (_tcschr(_T(".:\\"), first[cl]))
		{
			TCHAR tmp = *cp;
			*cp = _T('\0');
			nointernal = IsExistingFile(first);
			*cp = tmp;
			break;
		}
	}

	/* Scan internal command table */
	for (cmdptr = cmds; !nointernal && cmdptr->name; cmdptr++)
	{
		if (!_tcsnicmp(first, cmdptr->name, cl) && cmdptr->name[cl] == _T('\0'))
		{
			StringCchCopyW(com, CMDLINE_LENGTH, first);
			StringCchCatW(com, CMDLINE_LENGTH, rest);
			param = &com[cl];

			/* Skip over whitespace to rest of line, exclude 'echo' command */
			if (_tcsicmp(cmdptr->name, _T("echo")) != 0)
				while (_istspace(*param))
					param++;
			ret = cmdptr->func(param);
			cmd_free(com);
			return ret;
		}
	}

	ret = Execute(com, first, rest, Cmd);
	cmd_free(com);
	return ret;
}


/*
 * process the command line and execute the appropriate functions
 * full input/output redirection and piping are supported
 */
INT ParseCommandLine(LPTSTR cmd)
{
	INT Ret = 0;
	PARSED_COMMAND *Cmd = ParseCommand(cmd);
	if (Cmd)
	{
		Ret = ExecuteCommand(Cmd);
		FreeCommand(Cmd);
	}
	return Ret;
}

/* Execute a command without waiting for it to finish. If it's an internal
 * command or batch file, we must create a new cmd.exe process to handle it.
 */
static HANDLE
ExecuteAsync(PARSED_COMMAND *Cmd)
{
	TCHAR CmdPath[MAX_PATH];
	TCHAR CmdParams[CMDLINE_LENGTH], *ParamsEnd;
	STARTUPINFO stui;
	PROCESS_INFORMATION prci;

	/* Get the path to cmd.exe */
	GetModuleFileName(NULL, CmdPath, ARRAYSIZE(CmdPath));

	/* Build the parameter string to pass to cmd.exe */
	StringCchCopyW(CmdParams, CMDLINE_LENGTH, _T("/S/D/C\""));
    ParamsEnd = CmdParams + _tcslen(CmdParams);
	ParamsEnd = Unparse(Cmd, ParamsEnd, &CmdParams[CMDLINE_LENGTH - 2]);
	if (!ParamsEnd)
	{
		error_out_of_memory();
		return NULL;
	}
	StringCchCopyW(ParamsEnd, CMDLINE_LENGTH - (ParamsEnd - CmdParams), _T("\""));
	memset(&stui, 0, sizeof stui);
	stui.cb = sizeof(STARTUPINFO);
	if (!CreateProcess(CmdPath, CmdParams, NULL, NULL, TRUE, 0,
	                   NULL, NULL, &stui, &prci))
	{
		ErrorMessage(GetLastError(), NULL);
		return NULL;
	}

	CloseHandle(prci.hThread);
	return prci.hProcess;
}

static INT
ExecutePipeline(PARSED_COMMAND *Cmd)
{
#ifdef FEATURE_REDIRECTION
	HANDLE hInput = NULL;
	HANDLE hOldConIn = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hOldConOut = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE hProcess[MAXIMUM_WAIT_OBJECTS];
	INT nProcesses = 0;
	DWORD dwExitCode;

	/* Do all but the last pipe command */
	do
	{
		HANDLE hPipeRead, hPipeWrite;
		if (nProcesses > (MAXIMUM_WAIT_OBJECTS - 2))
		{
			error_too_many_parameters(_T("|"));
			goto failed;
		}

		/* Create the pipe that this process will write into. */
		if (!CreatePipe(&hPipeRead, &hPipeWrite, NULL, 0))
		{
			error_no_pipe();
			goto failed;
		}

		SetHandleInformation(hPipeWrite, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
		SetStdHandle(STD_OUTPUT_HANDLE, hPipeWrite);

		hProcess[nProcesses] = ExecuteAsync(Cmd->Subcommands);
		CloseHandle(hPipeWrite);
		if (hInput)
			CloseHandle(hInput);

		SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
		SetStdHandle(STD_INPUT_HANDLE, hPipeRead);
		hInput = hPipeRead;

		if (!hProcess[nProcesses])
			goto failed;
		nProcesses++;

		Cmd = Cmd->Subcommands->Next;
	} while (Cmd->Type == C_PIPE);

	/* The last process uses the original STDOUT */
	SetStdHandle(STD_OUTPUT_HANDLE, hOldConOut);
	hProcess[nProcesses] = ExecuteAsync(Cmd);
	if (!hProcess[nProcesses])
		goto failed;
	nProcesses++;
	CloseHandle(hInput);
	SetStdHandle(STD_INPUT_HANDLE, hOldConIn);

	EnterCriticalSection(&ChildProcessRunningLock);
	WaitForMultipleObjects(nProcesses, hProcess, TRUE, INFINITE);
	LeaveCriticalSection(&ChildProcessRunningLock);

	GetExitCodeProcess(hProcess[nProcesses - 1], &dwExitCode);
	nErrorLevel = (INT)dwExitCode;

	while (--nProcesses >= 0)
		CloseHandle(hProcess[nProcesses]);
	return nErrorLevel;

failed:
	if (hInput)
		CloseHandle(hInput);
	while (--nProcesses >= 0)
	{
		TerminateProcess(hProcess[nProcesses], 0);
		CloseHandle(hProcess[nProcesses]);
	}
	SetStdHandle(STD_INPUT_HANDLE, hOldConIn);
	SetStdHandle(STD_OUTPUT_HANDLE, hOldConOut);
#endif

    return nErrorLevel;
}

INT
ExecuteCommand(PARSED_COMMAND *Cmd)
{
	PARSED_COMMAND *Sub;
	LPTSTR First, Rest;
	INT Ret = 0;

	if (!PerformRedirection(Cmd->Redirections))
		return 1;
	switch (Cmd->Type)
	{
	case C_COMMAND:
		Ret = 1;
		First = DoDelayedExpansion(Cmd->Command.First);
		if (First)
		{
			Rest = DoDelayedExpansion(Cmd->Command.Rest);
			if (Rest)
			{
				Ret = DoCommand(First, Rest, Cmd);
				cmd_free(Rest);
			}
			cmd_free(First);
		}
		break;
	case C_QUIET:
	case C_BLOCK:
	case C_MULTI:
		for (Sub = Cmd->Subcommands; Sub; Sub = Sub->Next)
			Ret = ExecuteCommand(Sub);
		break;
	case C_IFFAILURE:
		Sub = Cmd->Subcommands;
		Ret = ExecuteCommand(Sub);
		if (Ret != 0)
		{
			nErrorLevel = Ret;
			Ret = ExecuteCommand(Sub->Next);
		}
		break;
	case C_IFSUCCESS:
		Sub = Cmd->Subcommands;
		Ret = ExecuteCommand(Sub);
		if (Ret == 0)
			Ret = ExecuteCommand(Sub->Next);
		break;
	case C_PIPE:
		ExecutePipeline(Cmd);
		break;
	case C_IF:
		Ret = ExecuteIf(Cmd);
		break;
	case C_FOR:
		Ret = ExecuteFor(Cmd);
		break;
	}

	UndoRedirection(Cmd->Redirections, NULL);
	return Ret;
}

LPTSTR
GetEnvVar(LPCTSTR varName)
{
	static LPTSTR ret = NULL;
	UINT size;

	cmd_free(ret);
	ret = NULL;
	size = GetEnvironmentVariable(varName, NULL, 0);
	if (size > 0)
	{
		ret = cmd_alloc(size * sizeof(TCHAR));
		if (ret != NULL)
			GetEnvironmentVariable(varName, ret, size);
	}
	return ret;
}

LPCTSTR
GetEnvVarOrSpecial(LPCTSTR varName)
{
	static TCHAR ret[MAX_PATH];
	LPTSTR var = GetEnvVar(varName);
	if (var)
		return var;

	if (_tcsicmp(varName,_T("cd")) ==0)
	{
		GetCurrentDirectory(MAX_PATH, ret);
		return ret;
	}
	else if (_tcsicmp(varName,_T("time")) ==0)
	{
		return GetTimeString();
	}
	else if (_tcsicmp(varName,_T("date")) ==0)
	{
		return GetDateString();
	}
	else if (_tcsicmp(varName,_T("random")) ==0)
	{
		_itow(rand(),ret,10);
		return ret;
	}
	else if (_tcsicmp(varName,_T("cmdcmdline")) ==0)
	{
		return GetCommandLine();
	}
	else if (_tcsicmp(varName,_T("cmdextversion")) ==0)
	{
		_itow(2,ret,10);
		return ret;
	}
	else if (_tcsicmp(varName,_T("errorlevel")) ==0)
	{
		_itow(nErrorLevel,ret,10);
		return ret;
	}

	return NULL;
}

static LPTSTR
GetEnhancedVar(TCHAR **pFormat, LPTSTR (*GetVar)(TCHAR, BOOL *))
{
	static const TCHAR ModifierTable[] = _T("dpnxfsatz");
	enum {
		M_DRIVE = 1, M_PATH  = 2, M_NAME  = 4, M_EXT   = 8, M_FULL  = 16,
		M_SHORT = 32, M_ATTR  = 64, M_TIME  = 128, M_SIZE  = 256,
	} Modifiers = 0;

	TCHAR *Format, *FormatEnd;
	TCHAR *PathVarName = NULL;
	LPTSTR Variable;
	TCHAR *VarEnd;
	BOOL VariableIsParam0;
	TCHAR FullPath[MAX_PATH];
	TCHAR FixedPath[MAX_PATH], *Filename, *Extension;
	HANDLE hFind;
	WIN32_FIND_DATA w32fd;
	TCHAR *In, *Out;
	static TCHAR Result[CMDLINE_LENGTH];

	FormatEnd = Format = *pFormat;
	while (*FormatEnd && _tcschr(ModifierTable, _totlower(*FormatEnd)))
		FormatEnd++;
	if (*FormatEnd == _T('$'))
	{
		PathVarName = FormatEnd + 1;
		FormatEnd = _tcschr(PathVarName, _T(':'));
		if (!FormatEnd) return NULL;
		Variable = GetVar(*++FormatEnd, &VariableIsParam0);
		if (!Variable) return NULL;
	}
	else
	{
		while (!(Variable = GetVar(*FormatEnd, &VariableIsParam0)))
		{
			if (FormatEnd == Format) return NULL;
			FormatEnd--;
		}
	}

	for (; Format < FormatEnd && *Format != _T('$'); Format++)
		Modifiers |= 1 << (_tcschr(ModifierTable, _totlower(*Format)) - ModifierTable);

	*pFormat = FormatEnd + 1;

	VarEnd = &Variable[_tcslen(Variable)];
	if (*Variable == _T('"'))
	{
		Variable++;
		if (VarEnd > Variable && VarEnd[-1] == _T('"'))
			VarEnd--;
	}

	if ((char *)VarEnd - (char *)Variable >= sizeof Result) return _T("");
	memcpy(Result, Variable, (char *)VarEnd - (char *)Variable);
	Result[VarEnd - Variable] = _T('\0');

	if (PathVarName)
	{
		LPTSTR PathVar;
		FormatEnd[-1] = _T('\0');
		PathVar = GetEnvVar(PathVarName);
		FormatEnd[-1] = _T(':');
		if (!PathVar || !SearchPath(PathVar, Result, NULL, MAX_PATH, FullPath, NULL))
			return _T("");
	}
	else if (Modifiers == 0)
	{
		return Result;
	}
	else if (VariableIsParam0)
	{
		StringCchCopyW(FullPath, MAX_PATH, bc->BatchFilePath);
	}
	else
	{
		if (!GetFullPathName(Result, MAX_PATH, FullPath, NULL))
			return _T("");
	}

	In = FullPath;
	Out = FixedPath;

	*Out++ = *In++;
	*Out++ = *In++;
	*Out++ = *In++;
	do {
		TCHAR *Next = _tcschr(In, _T('\\'));
		if (Next) *Next++ = _T('\0');
		if (Out + _tcslen(In) + 1 >= &FixedPath[MAX_PATH]) return _T("");
		StringCchCopyW(Out, &FixedPath[MAX_PATH] - Out, In);
		hFind = FindFirstFile(FixedPath, &w32fd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			LPTSTR FixedComponent = w32fd.cFileName;
			if (*w32fd.cAlternateFileName &&
			    ((Modifiers & M_SHORT) || !_tcsicmp(In, w32fd.cAlternateFileName)))
			{
				FixedComponent = w32fd.cAlternateFileName;
			}
			FindClose(hFind);
			if (Out + _tcslen(FixedComponent) + 1 >= &FixedPath[MAX_PATH])
				return _T("");
			StringCchCopyW(Out, &FixedPath[MAX_PATH] - Out, FixedComponent);
		}
		Filename = Out;
		Out += _tcslen(Out);
		*Out++ = _T('\\');
		In = Next;
	} while (In != NULL);
	Out[-1] = _T('\0');

	Out = Result;
	if (hFind != INVALID_HANDLE_VALUE)
	{
		if (Modifiers & M_ATTR)
		{
			static const struct { TCHAR C; WORD V; } *A, T[] = {
				{_T('d'),FILE_ATTRIBUTE_DIRECTORY},{_T('r'),FILE_ATTRIBUTE_READONLY},
				{_T('a'),FILE_ATTRIBUTE_ARCHIVE},{_T('h'),FILE_ATTRIBUTE_HIDDEN},
				{_T('s'),FILE_ATTRIBUTE_SYSTEM},{_T('c'),FILE_ATTRIBUTE_COMPRESSED},
				{_T('o'),FILE_ATTRIBUTE_OFFLINE},{_T('t'),FILE_ATTRIBUTE_TEMPORARY},
				{_T('l'),FILE_ATTRIBUTE_REPARSE_POINT},
			};
			for (A = T; A != &T[9]; A++)
				*Out++ = w32fd.dwFileAttributes & A->V ? A->C : _T('-');
			*Out++ = _T(' ');
		}
		if (Modifiers & M_TIME)
		{
			FILETIME ft;
			SYSTEMTIME st;
			FileTimeToLocalFileTime(&w32fd.ftLastWriteTime, &ft);
			FileTimeToSystemTime(&ft, &st);
			Out += FormatDate(Out, &st, TRUE);
			*Out++ = _T(' ');
			Out += FormatTime(Out, &st);
			*Out++ = _T(' ');
		}
		if (Modifiers & M_SIZE)
		{
			ULARGE_INTEGER Size;
			Size.LowPart = w32fd.nFileSizeLow;
			Size.HighPart = w32fd.nFileSizeHigh;
			StringCchPrintfW(Out, &Result[CMDLINE_LENGTH] - Out, _T("%I64u "), Size.QuadPart);
            Out += _tcslen(Out);
		}
	}

	if (PathVarName || (Modifiers & M_SHORT))
		if ((Modifiers & (M_DRIVE | M_PATH | M_NAME | M_EXT)) == 0)
			Modifiers |= M_FULL;

	Extension = _tcsrchr(Filename, _T('.'));
	if (Modifiers & (M_DRIVE | M_FULL))
	{
		*Out++ = FixedPath[0];
		*Out++ = FixedPath[1];
	}
	if (Modifiers & (M_PATH | M_FULL))
	{
		memcpy(Out, &FixedPath[2], (char *)Filename - (char *)&FixedPath[2]);
		Out += Filename - &FixedPath[2];
	}
	if (Modifiers & (M_NAME | M_FULL))
	{
		while (*Filename && Filename != Extension)
			*Out++ = *Filename++;
	}
	if (Modifiers & (M_EXT | M_FULL))
	{
		if (Extension)
			StringCchCopyW(Out, &Result[CMDLINE_LENGTH] - Out, Extension);
	}

	while (Out != &Result[0] && Out[-1] == _T(' '))
		Out--;
	*Out = _T('\0');

	return Result;
}

LPCTSTR
GetBatchVar(TCHAR *varName, UINT *varNameLen)
{
	LPCTSTR ret;
	TCHAR *varNameEnd;
	BOOL dummy;

	*varNameLen = 1;

	switch ( *varName )
	{
	case _T('~'):
		varNameEnd = varName + 1;
		ret = GetEnhancedVar(&varNameEnd, FindArg);
		if (!ret)
		{
			error_syntax(varName);
			return NULL;
		}
		*varNameLen = varNameEnd - varName;
		return ret;
	case _T('0'): case _T('1'): case _T('2'): case _T('3'): case _T('4'):
	case _T('5'): case _T('6'): case _T('7'): case _T('8'): case _T('9'):
		return FindArg(*varName, &dummy);
    case _T('*'):
        return bc->raw_params;
	case _T('%'):
		return _T("%");
	}
	return NULL;
}

BOOL
SubstituteVars(TCHAR *Src, TCHAR *Dest, TCHAR Delim)
{
#define APPEND(From, Length) { \
	if (Dest + (Length) > DestEnd) \
		goto too_long; \
	memcpy(Dest, From, (Length) * sizeof(TCHAR)); \
	Dest += Length; }
#define APPEND1(Char) { \
	if (Dest >= DestEnd) \
		goto too_long; \
	*Dest++ = Char; }

	TCHAR *DestEnd = Dest + CMDLINE_LENGTH - 1;
	const TCHAR *Var;
	int VarLength;
	TCHAR *SubstStart;
	TCHAR EndChr;
	while (*Src)
	{
		if (*Src != Delim)
		{
			APPEND1(*Src++)
			continue;
		}

		Src++;
		if (bc && Delim == _T('%'))
		{
			UINT NameLen;
			Var = GetBatchVar(Src, &NameLen);
			if (Var != NULL)
			{
				VarLength = _tcslen(Var);
				APPEND(Var, VarLength)
				Src += NameLen;
				continue;
			}
		}

		SubstStart = Src;
		while (*Src != Delim && !(*Src == _T(':') && Src[1] != Delim))
		{
			if (!*Src) goto bad_subst;
			Src++;
		}

		EndChr = *Src;
		*Src = _T('\0');
		Var = GetEnvVarOrSpecial(SubstStart);
		*Src++ = EndChr;
		if (Var == NULL)
		{
			if (bc) continue;
			goto bad_subst;
		}
		VarLength = _tcslen(Var);

		if (EndChr == Delim)
		{
			APPEND(Var, VarLength)
		}
		else if (*Src == _T('~'))
		{
			int Start = _tcstol(Src + 1, &Src, 0);
			int End = VarLength;
			if (Start < 0) Start += VarLength;
			Start = max(Start, 0);
			Start = min(Start, VarLength);
			if (*Src == _T(','))
			{
				End = _tcstol(Src + 1, &Src, 0);
				End += (End < 0) ? VarLength : Start;
				End = max(End, Start);
				End = min(End, VarLength);
			}
			if (*Src++ != Delim) goto bad_subst;
			APPEND(&Var[Start], End - Start);
		}
		else
		{
			TCHAR *Old, *New;
			DWORD OldLength, NewLength;
			BOOL Star = FALSE;
			int LastMatch = 0, i = 0;

			if (*Src == _T('*'))
			{
				Star = TRUE;
				Src++;
			}

			Src = _tcschr(Old = Src, _T('='));
			if (Src == NULL) goto bad_subst;
			OldLength = Src++ - Old;
			if (OldLength == 0) goto bad_subst;

			Src = _tcschr(New = Src, Delim);
			if (Src == NULL) goto bad_subst;
			NewLength = Src++ - New;

			while (i < VarLength)
			{
				if (_tcsnicmp(&Var[i], Old, OldLength) == 0)
				{
					if (!Star)
						APPEND(&Var[LastMatch], i - LastMatch)
					APPEND(New, NewLength)
					i += OldLength;
					LastMatch = i;
					if (Star) break;
					continue;
				}
				i++;
			}
			APPEND(&Var[LastMatch], VarLength - LastMatch)
		}
		continue;

	bad_subst:
		Src = SubstStart;
		if (!bc)
			APPEND1(Delim)
	}
	*Dest = _T('\0');
	return TRUE;
too_long:
	wprintf(L"The input line is too long.\n");
	nErrorLevel = 9023;
	return FALSE;
#undef APPEND
#undef APPEND1
}

static LPTSTR FindForVar(TCHAR Var, BOOL *IsParam0)
{
	FOR_CONTEXT *Ctx;
	*IsParam0 = FALSE;
	for (Ctx = fc; Ctx != NULL; Ctx = Ctx->prev)
    {
        if ((UINT)(Var - Ctx->firstvar) < Ctx->varcount)
            return Ctx->values[Var - Ctx->firstvar];
    }
    return NULL;
}

BOOL
SubstituteForVars(TCHAR *Src, TCHAR *Dest)
{
	TCHAR *DestEnd = &Dest[CMDLINE_LENGTH - 1];
	while (*Src)
	{
		if (Src[0] == _T('%'))
		{
			BOOL Dummy;
			LPTSTR End = &Src[2];
			LPTSTR Value = NULL;

			if (Src[1] == _T('~'))
				Value = GetEnhancedVar(&End, FindForVar);

			if (!Value)
				Value = FindForVar(Src[1], &Dummy);

			if (Value)
			{
				if (Dest + _tcslen(Value) > DestEnd)
					return FALSE;
				StringCchCopyW(Dest, &Dest[CMDLINE_LENGTH - 1] - Dest, Value);
                Dest += _tcslen(Value);
				Src = End;
				continue;
			}
		}
		if (Dest >= DestEnd) return FALSE;
		*Dest++ = *Src++;
	}
	*Dest = _T('\0');
	return TRUE;
}

LPTSTR
DoDelayedExpansion(LPTSTR Line)
{
	TCHAR Buf1[CMDLINE_LENGTH];
	TCHAR Buf2[CMDLINE_LENGTH];

	if (!SubstituteForVars(Line, Buf1))
		return NULL;

	if (!bDelayedExpansion || !_tcschr(Buf1, _T('!')))
		return cmd_dup(Buf1);

	if (!SubstituteVars(Buf1, Buf2, _T('!')))
		return NULL;
	return cmd_dup(Buf2);
}

BOOL
ReadLine (TCHAR *commandline, BOOL bMore)
{
	TCHAR readline[CMDLINE_LENGTH];
	LPTSTR ip;

	if (bc == NULL)
	{
		if (bMore)
		{
			wprintf(L"More? ");
		}
		else
		{
			if (bEcho)
			{
				if (!bIgnoreEcho)
					wprintf(L"\n");
				PrintPrompt();
			}
		}

		if (!ReadCommand(readline, CMDLINE_LENGTH - 1))
		{
			bExit = TRUE;
			return FALSE;
		}

        if (readline[0] == _T('\0'))
            wprintf(L"\n");

        if (CheckCtrlBreak(BREAK_INPUT))
            return FALSE;

        if (readline[0] == _T('\0'))
            return FALSE;

        ip = readline;
	}
	else
	{
		ip = ReadBatchLine();
		if (!ip)
			return FALSE;
	}

	return SubstituteVars(ip, commandline, _T('%'));
}

static VOID
ProcessInput(VOID)
{
	PARSED_COMMAND *Cmd;

    while (!bCanExit || !bExit)
    {
        bCtrlBreak = FALSE;

        Cmd = ParseCommand(NULL);
        if (!Cmd)
            continue;

        ExecuteCommand(Cmd);
        FreeCommand(Cmd);
    }
}

BOOL WINAPI BreakHandler(DWORD dwCtrlType)
{
	DWORD			dwWritten;
	INPUT_RECORD	rec;

 	if ((dwCtrlType != CTRL_C_EVENT) &&
	    (dwCtrlType != CTRL_BREAK_EVENT))
	{
		return FALSE;
    }

    if (!TryEnterCriticalSection(&ChildProcessRunningLock))
    {
        return TRUE;
    }
    else
	{
        LeaveCriticalSection(&ChildProcessRunningLock);
    }

    bCtrlBreak = TRUE;

    rec.EventType = KEY_EVENT;
    rec.Event.KeyEvent.bKeyDown = TRUE;
    rec.Event.KeyEvent.wRepeatCount = 1;
    rec.Event.KeyEvent.wVirtualKeyCode = _T('C');
    rec.Event.KeyEvent.wVirtualScanCode = _T('C') - 35;
    rec.Event.KeyEvent.uChar.AsciiChar = 'C';
    rec.Event.KeyEvent.uChar.UnicodeChar = L'C';
    rec.Event.KeyEvent.dwControlKeyState = RIGHT_CTRL_PRESSED;

    WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &rec, 1, &dwWritten);
    return TRUE;
}


VOID AddBreakHandler(VOID)
{
	SetConsoleCtrlHandler(BreakHandler, TRUE);
}


VOID RemoveBreakHandler(VOID)
{
	SetConsoleCtrlHandler(BreakHandler, FALSE);
}

static VOID
LoadRegistrySettings(HKEY hKeyRoot)
{
    LONG lRet;
    HKEY hKey;
    DWORD dwType, len;
    DWORD Buffer[6];

    lRet = RegOpenKeyEx(hKeyRoot,
                        _T("Software\\Microsoft\\Command Processor"),
                        0,
                        KEY_QUERY_VALUE,
                        &hKey);
    if (lRet != ERROR_SUCCESS) return;

#ifdef INCLUDE_CMD_COLOR
    len = sizeof(Buffer);
    lRet = RegQueryValueEx(hKey, _T("DefaultColor"), NULL,
                           &dwType, (LPBYTE)&Buffer, &len);
    if (lRet == ERROR_SUCCESS)
    {
        if (dwType == REG_DWORD) wDefColor = (WORD)*(PDWORD)Buffer;
        else if (dwType == REG_SZ) wDefColor = (WORD)_tcstol((PTSTR)Buffer, NULL, 0);
    }
#endif

    len = sizeof(Buffer);
    lRet = RegQueryValueEx(hKey, _T("DelayedExpansion"), NULL,
                           &dwType, (LPBYTE)&Buffer, &len);
    if (lRet == ERROR_SUCCESS)
    {
        if (dwType == REG_DWORD) bDelayedExpansion = !!*(PDWORD)Buffer;
        else if (dwType == REG_SZ) bDelayedExpansion = (_ttol((PTSTR)Buffer) == 1);
    }

    len = sizeof(Buffer);
    lRet = RegQueryValueEx(hKey, _T("EnableExtensions"), NULL,
                           &dwType, (LPBYTE)&Buffer, &len);
    if (lRet == ERROR_SUCCESS)
    {
        if (dwType == REG_DWORD) bEnableExtensions = !!*(PDWORD)Buffer;
        else if (dwType == REG_SZ) bEnableExtensions = (_ttol((PTSTR)Buffer) == 1);
    }

    len = sizeof(Buffer);
    lRet = RegQueryValueEx(hKey, _T("CompletionChar"), NULL,
                           &dwType, (LPBYTE)&Buffer, &len);
    if (lRet == ERROR_SUCCESS)
    {
        if (dwType == REG_DWORD) AutoCompletionChar = (TCHAR)*(PDWORD)Buffer;
        else if (dwType == REG_SZ) AutoCompletionChar = (TCHAR)_tcstol((PTSTR)Buffer, NULL, 0);
    }

    if (IS_COMPLETION_DISABLED(AutoCompletionChar))
        AutoCompletionChar = 0x20;

    len = sizeof(Buffer);
    lRet = RegQueryValueEx(hKey, _T("PathCompletionChar"), NULL,
                           &dwType, (LPBYTE)&Buffer, &len);
    if (lRet == ERROR_SUCCESS)
    {
        if (dwType == REG_DWORD) PathCompletionChar = (TCHAR)*(PDWORD)Buffer;
        else if (dwType == REG_SZ) PathCompletionChar = (TCHAR)_tcstol((PTSTR)Buffer, NULL, 0);
    }

    if (IS_COMPLETION_DISABLED(PathCompletionChar))
        PathCompletionChar = 0x20;

    if (PathCompletionChar >= 0x20 && AutoCompletionChar < 0x20)
        PathCompletionChar = AutoCompletionChar;
    else if (AutoCompletionChar >= 0x20 && PathCompletionChar < 0x20)
        AutoCompletionChar = PathCompletionChar;

    RegCloseKey(hKey);
}

static VOID
ExecuteAutoRunFile(HKEY hKeyRoot)
{
    LONG lRet;
    HKEY hKey;
    DWORD dwType, len;
    TCHAR AutoRun[2048];

    lRet = RegOpenKeyEx(hKeyRoot,
                        _T("Software\\Microsoft\\Command Processor"),
                        0, KEY_QUERY_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) return;

    len = sizeof(AutoRun);
    lRet = RegQueryValueEx(hKey, _T("AutoRun"), NULL,
                           &dwType, (LPBYTE)&AutoRun, &len);
    if ((lRet == ERROR_SUCCESS) && (dwType == REG_EXPAND_SZ || dwType == REG_SZ))
    {
        if (*AutoRun) ParseCommandLine(AutoRun);
    }

    RegCloseKey(hKey);
}

static VOID
GetCmdLineCommand(TCHAR *commandline, TCHAR *ptr, BOOL AlwaysStrip)
{
    TCHAR *LastQuote;

    while (_istspace(*ptr)) ptr++;

    if (*ptr == _T('"') && (LastQuote = _tcsrchr(++ptr, _T('"'))) != NULL)
    {
        TCHAR *Space;
        *LastQuote = _T('\0');
        for (Space = ptr + 1; Space < LastQuote; Space++)
        {
            if (_istspace(*Space))
            {
                if (!AlwaysStrip && !_tcspbrk(ptr, _T("\"&<>@^|")) && SearchForExecutable(ptr, commandline))
                {
                    *LastQuote = _T('"');
                    StringCchCopyW(commandline, CMDLINE_LENGTH, ptr - 1);
                    return;
                }
                break;
            }
        }
        StringCchCopyW(commandline, CMDLINE_LENGTH, ptr);
        StringCchCatW(commandline, CMDLINE_LENGTH, LastQuote + 1);
        return;
    }
    StringCchCopyW(commandline, CMDLINE_LENGTH, ptr);
}

static VOID
Initialize()
{
    HMODULE NtDllModule;
    TCHAR commandline[CMDLINE_LENGTH];
    TCHAR ModuleName[_MAX_PATH + 1];
    UINT InputCodePage, OutputCodePage;

    TCHAR *ptr, *cmdLine, option = 0;
    BOOL AlwaysStrip = FALSE;
    BOOL AutoRun = TRUE;

    InitOSVersion();

    NtDllModule = GetModuleHandle(TEXT("ntdll.dll"));
    if (NtDllModule != NULL)
    {
        NtQueryInformationProcessPtr = (NtQueryInformationProcessProc)GetProcAddress(NtDllModule, "NtQueryInformationProcess");
        NtReadVirtualMemoryPtr = (NtReadVirtualMemoryProc)GetProcAddress(NtDllModule, "NtReadVirtualMemory");
    }

    LoadRegistrySettings(HKEY_LOCAL_MACHINE);
    LoadRegistrySettings(HKEY_CURRENT_USER);

    InitLocale();
    InitPrompt();
#ifdef FEATURE_DIR_STACK
    InitDirectoryStack();
#endif
#ifdef FEATURE_HISTORY
    InitHistory();
#endif

    if (GetModuleFileName(NULL, ModuleName, ARRAYSIZE(ModuleName)) != 0)
    {
        ModuleName[_MAX_PATH] = _T('\0');
        SetEnvironmentVariable (_T("COMSPEC"), ModuleName);
    }

    AddBreakHandler();

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn  = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hOut, 0);
    SetConsoleMode(hOut, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    SetConsoleMode(hIn , ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    InputCodePage  = GetConsoleCP();
    OutputCodePage = GetConsoleOutputCP();

    cmdLine = GetCommandLine();
    TRACE ("[command args: %s]\n", cmdLine);

    for (ptr = cmdLine; *ptr; ptr++)
    {
        if (*ptr == _T('/'))
        {
            option = _totupper(ptr[1]);
            if (option == _T('?'))
            {
                wprintf(L"Starts a new instance of the ReactOS command interpreter.\n\n"
                        L"CMD [/A | /U] [/Q] [/D] [/E:ON | /E:OFF] [/F:ON | /F:OFF] [/V:ON | /V:OFF]\n"
                        L"    [[/S] [/C | /K] string]\n\n"
                        L"/C      Carries out the command specified by string and then terminates.\n"
                        L"/K      Carries out the command specified by string but remains.\n"
                        L"/S      Modifies the treatment of string after /C or /K (see below).\n"
                        L"/Q      Turns echo off.\n"
                        L"/D      Disable execution of AutoRun commands from registry (see below).\n"
                        L"/A      Formats output of internal commands to a pipe or file as ANSI.\n"
                        L"/U      Formats output of internal commands to a pipe or file as Unicode.\n"
                        L"/E:ON   Enable command extensions (see below).\n"
                        L"/E:OFF  Disable command extensions (see below).\n"
                        L"/F:ON   Enable file and directory name completion (see below).\n"
                        L"/F:OFF  Disable file and directory name completion (see below).\n"
                        L"/V:ON   Enable delayed environment variable expansion (see below).\n"
                        L"/V:OFF  Disable delayed environment variable expansion (see below).\n");
                nErrorLevel = 1;
                bExit = TRUE;
                return;
            }
            else if (option == _T('P'))
            {
                if (!IsExistingFile (_T("\\autoexec.bat")))
                {
#ifdef INCLUDE_CMD_DATE
                    cmd_date (_T(""));
#endif
#ifdef INCLUDE_CMD_TIME
                    cmd_time (_T(""));
#endif
                }
                else
                {
                    ParseCommandLine (_T("\\autoexec.bat"));
                }
                bCanExit = FALSE;
            }
            else if (option == _T('A'))
            {
                //OutputStreamMode = AnsiText;
            }
            else if (option == _T('C') || option == _T('K') || option == _T('R'))
            {
                break;
            }
            else if (option == _T('D'))
            {
                AutoRun = FALSE;
            }
            else if (option == _T('Q'))
            {
                bDisableBatchEcho = TRUE;
            }
            else if (option == _T('S'))
            {
                AlwaysStrip = TRUE;
            }
#ifdef INCLUDE_CMD_COLOR
            else if (!_tcsnicmp(ptr, _T("/T:"), 3))
            {
                wDefColor = (WORD)_tcstoul(&ptr[3], &ptr, 16);
            }
#endif
            else if (option == _T('U'))
            {
                //OutputStreamMode = UTF16Text;
            }
            else if (option == _T('V'))
            {
                bDelayedExpansion = _tcsnicmp(&ptr[2], _T(":OFF"), 4);
            }
            else if (option == _T('E'))
            {
                bEnableExtensions = _tcsnicmp(&ptr[2], _T(":OFF"), 4);
            }
            else if (option == _T('X'))
            {
                bEnableExtensions = TRUE;
            }
            else if (option == _T('Y'))
            {
                bEnableExtensions = FALSE;
            }
        }
    }

#ifdef INCLUDE_CMD_COLOR
    if (wDefColor == 0)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
            wDefColor = csbi.wAttributes;
    }

    if (wDefColor != 0)
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wDefColor);
#endif

    if (!*ptr)
    {
        wprintf(L"\nReactOS %s (Build %s)\n", KERNEL_VERSION_STR, KERNEL_VERSION_BUILD_STR);
        wprintf(L"(C) Copyright 1998-%s ReactOS Team.\n", COPYRIGHT_YEAR);
    }

    if (AutoRun)
    {
        ExecuteAutoRunFile(HKEY_LOCAL_MACHINE);
        ExecuteAutoRunFile(HKEY_CURRENT_USER);
    }

    if (*ptr)
    {
        GetCmdLineCommand(commandline, &ptr[2], AlwaysStrip);
        bWaitForCommand = TRUE;
        ParseCommandLine(commandline);
        bWaitForCommand = FALSE;
        if (option != _T('K'))
        {
            bExit = TRUE;
        }
    }
}


static VOID Cleanup()
{
    if (IsExistingFile(_T("cmdexit.bat")))
    {
        fwprintf(stderr, L"Cannot find 'cmdexit.bat'.\n");
        ParseCommandLine(_T("cmdexit.bat"));
    }
    else if (IsExistingFile(_T("\\cmdexit.bat")))
    {
        fwprintf(stderr, L"Cannot find 'cmdexit.bat'.\n");
        ParseCommandLine(_T("\\cmdexit.bat"));
    }

#ifdef FEATURE_DIRECTORY_STACK
    DestroyDirectoryStack();
#endif

#ifdef FEATURE_HISTORY
    CleanHistory();
#endif

    GetEnvVar(NULL);
    RemoveBreakHandler();

    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);

    DeleteCriticalSection(&ChildProcessRunningLock);
}

int _tmain(int argc, const TCHAR *argv[])
{
    TCHAR startPath[MAX_PATH];
    UINT InputCodePage, OutputCodePage;

    InitializeCriticalSection(&ChildProcessRunningLock);
    lpOriginalEnvironment = DuplicateEnvironment();

    GetCurrentDirectory(ARRAYSIZE(startPath), startPath);
    _tchdir(startPath);

    SetFileApisToOEM();
    InputCodePage  = GetConsoleCP();
    OutputCodePage = GetConsoleOutputCP();

    CMD_ModuleHandle = GetModuleHandle(NULL);

    Initialize();
    ProcessInput();
    Cleanup();

    cmd_free(lpOriginalEnvironment);

    cmd_exit(nErrorLevel);
    return nErrorLevel;
}

/* EOF */
