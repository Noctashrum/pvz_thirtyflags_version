#include <stdio.h>
#include "TodDebug.h"
#include "StackWalk.h"

PGET_LINE_FROM_ADDR_ROUTINE			gSymGetLineFromAddr;
PGET_SYM_FROM_ADDR_ROUTINE			gSymGetSymFromAddr;
PGET_MODULE_BASE_ROUTINE			gSymGetModuleBase;
PFUNCTION_TABLE_ACCESS_ROUTINE		gSymFunctionTableAccess;
PSTACK_WALK_ROUTINE					gStackWalk;
PCLEAN_UP_ROUTINE					gSymCleanUp;
PUNDECORATE_SYMBOL_NAME_ROUTINE		gUnDecorateSymbolName;
PSET_OPTIONS_ROUTINE				gSymSetOptions;
PINITIALIZE_ROUTINE					gSymInitialize;
PMINI_DUMP_WRITE_DUMP_ROUTINE		gMiniDumpWriteDump;
HINSTANCE							gDbgHelpModule;

bool StackWalkInitialize()
{
	if (gDbgHelpModule)
		return true;

	gDbgHelpModule = LoadLibrary("DBGHELP.DLL");
	if (gDbgHelpModule == NULL)
		return false;

	//define GetAddr(name, type) g##name = (type)GetProcAddress(gDbgHelpModule, #name);
	gMiniDumpWriteDump = (PMINI_DUMP_WRITE_DUMP_ROUTINE)GetProcAddress(gDbgHelpModule, "MiniDumpWriteDump");
	// NOTE: the export name was misspelled "SymInitialzie", so this returned NULL and
	// every later gSymInitialize(...) call jumped to address 0. The name is correct now.
	gSymInitialize = (PINITIALIZE_ROUTINE)GetProcAddress(gDbgHelpModule, "SymInitialize");
	gSymSetOptions = (PSET_OPTIONS_ROUTINE)GetProcAddress(gDbgHelpModule, "SymSetOptions");
	gSymCleanUp = (PCLEAN_UP_ROUTINE)GetProcAddress(gDbgHelpModule, "SymCleanUp");
	gUnDecorateSymbolName = (PUNDECORATE_SYMBOL_NAME_ROUTINE)GetProcAddress(gDbgHelpModule, "UnDecorateSymbolName");
	gStackWalk = (PSTACK_WALK_ROUTINE)GetProcAddress(gDbgHelpModule, "StackWalk");
	gSymFunctionTableAccess = (PFUNCTION_TABLE_ACCESS_ROUTINE)GetProcAddress(gDbgHelpModule, "SymFunctionTableAccess");
	gSymGetModuleBase = (PGET_MODULE_BASE_ROUTINE)GetProcAddress(gDbgHelpModule, "SymGetModuleBase");
	gSymGetSymFromAddr = (PGET_SYM_FROM_ADDR_ROUTINE)GetProcAddress(gDbgHelpModule, "SymGetSymFromAddr");
	gSymGetLineFromAddr = (PGET_LINE_FROM_ADDR_ROUTINE)GetProcAddress(gDbgHelpModule, "SymGetLineFromAddr");
	return true;
}

static void sStackWalkTraceOrLog(bool theLogInsteadOfTrace, bool theUseExcelFormat, const char* theFormat, ...)
{
	char aBuffer[1024];
	if (theUseExcelFormat)
	{
		strcpy(aBuffer, "  ");
	}

	va_list argList;
	va_start(argList, theFormat);
	TodVsnprintfEnsureNewLine(theUseExcelFormat ? aBuffer + 2 : aBuffer, sizeof(aBuffer), theFormat, argList);
	va_end(argList);

	if (theUseExcelFormat && strlen(aBuffer) > 1)
	{
		aBuffer[strlen(aBuffer) - 1] = ',';
	}

	if (theLogInsteadOfTrace)
	{
		TodLogString(aBuffer);
	}
	else
	{
		OutputDebugStringA(aBuffer);
	}
}

void StackWalkShowMiniStack(StackWalkMiniStack* theMiniStack, bool theLogInsteadOfTrace, bool theUseExcelFormat)
{
	HANDLE hProcess = GetCurrentProcess();
	if (!StackWalkInitialize())
	{
		TodTrace("StackWalkInitialize failed");
		return;
	}

	if (!gSymInitialize(hProcess, NULL, TRUE))
	{
		sStackWalkTraceOrLog(theLogInsteadOfTrace, theUseExcelFormat, "SymInitialize failed code %d", GetLastError());
	}

	for (int i = 0; i < MINISTACK_MAX_FRAMES; i++)
	{
		DWORD addr = theMiniStack->mFrame[i];
		char aBuffer[1048] = { 0 };

		DWORD aOffsetFromSymbol;
		PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)aBuffer;
		pSym->Address = 0x18U;
		pSym->MaxNameLength = sizeof(aBuffer) - pSym->Address;
		if (!gSymGetSymFromAddr(hProcess, addr, &aOffsetFromSymbol, pSym))
		{
			sStackWalkTraceOrLog(theLogInsteadOfTrace, theUseExcelFormat, "unknown symbol addr 0x%x error %d", GetLastError());
		}
		else
		{
			char aUndName[1024] = { '\0' };
			gUnDecorateSymbolName(pSym->Name, aUndName, sizeof(aUndName), UNDNAME_NAME_ONLY);

			IMAGEHLP_LINE aLine;
			memset(&aLine, 0, sizeof(aLine));

			DWORD aDisplacement;
			if (!gSymGetLineFromAddr(hProcess, addr, &aDisplacement, &aLine))
			{
				sStackWalkTraceOrLog(theLogInsteadOfTrace, theUseExcelFormat, "unknown file addr 0x%x error %d", addr, GetLastError());
			}
			else
			{
				sStackWalkTraceOrLog(theLogInsteadOfTrace, theUseExcelFormat, "%s(%d): %s", aLine.FileName, aLine.LineNumber, pSym->Name);
			}
		}
	}

	if (theUseExcelFormat)
	{
		sStackWalkTraceOrLog(theLogInsteadOfTrace, false, "\n");
	}
	
	gSymCleanUp(hProcess);
}

static void sStackWalkCaptureMiniStackContext(StackWalkMiniStack* theMiniStack, PCONTEXT theContext, int theSkipFrames)
{
	if (!StackWalkInitialize())
	{
		TodTrace("StackWalkInitialize failed");
		return;
	}

	theMiniStack->mFrameCount = 0;
	HANDLE aProcess = GetCurrentProcess();
	HANDLE aThread = GetCurrentThread();

	STACKFRAME aStackFrame;
	memset(&aStackFrame, 0, sizeof(aStackFrame));
	aStackFrame.AddrPC.Offset = theContext->Eip;
	aStackFrame.AddrPC.Mode = ADDRESS_MODE::AddrModeFlat;
	aStackFrame.AddrFrame.Offset = theContext->Ebp;
	aStackFrame.AddrFrame.Mode = ADDRESS_MODE::AddrModeFlat;
	aStackFrame.AddrStack.Offset = theContext->Esp;
	aStackFrame.AddrStack.Mode = ADDRESS_MODE::AddrModeFlat;

	for (int i = 0; i < MINISTACK_MAX_FRAMES + theSkipFrames; i++)
	{
		if (!gStackWalk(IMAGE_FILE_MACHINE_I386, aProcess, aThread, &aStackFrame, theContext, NULL, gSymFunctionTableAccess, gSymGetModuleBase, NULL))
		{
			break;
		}

		if (i >= theSkipFrames)
		{
			theMiniStack->mFrame[theMiniStack->mFrameCount++] = aStackFrame.AddrPC.Offset;
		}
	}
}

void StackWalkLogExceptionStack(LPEXCEPTION_POINTERS theExceptionInfo)
{
	StackWalkMiniStack aMiniStack;
	sStackWalkCaptureMiniStackContext(&aMiniStack, theExceptionInfo->ContextRecord, 0);
	StackWalkShowMiniStack(&aMiniStack, true, false);
}

void MiniDump(LPEXCEPTION_POINTERS pExceptionInfo, const char* theName)
{
	if (!StackWalkInitialize())
	{
		TodTrace("StackWalkInitialize failed");
		return;
	}

	char aDumpFileName[MAX_PATH];
	sprintf(aDumpFileName, "%sGameMiniDump_%s.dmp", gDebugDataFolder, theName);
	HANDLE aFileHandle = CreateFileA(aDumpFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (aFileHandle == INVALID_HANDLE_VALUE)
	{
		TodTraceAndLog("Failed to create dump file '%s' (error %d)", aDumpFileName, GetLastError());
		return;
	}

	MINIDUMP_EXCEPTION_INFORMATION ExInfo;
	ExInfo.ThreadId = GetCurrentThreadId();
	ExInfo.ExceptionPointers = pExceptionInfo;
	ExInfo.ClientPointers = FALSE;
	if (gMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), aFileHandle, MINIDUMP_TYPE::MiniDumpNormal, &ExInfo, NULL, NULL))
	{
		TodTraceAndLog("Saved minidump file '%s'.", aDumpFileName);
	}
	else
	{
		TodTraceAndLog("Failed to write dump file '%s' (error %d)", aDumpFileName, GetLastError());
	}

	CloseHandle(aFileHandle);
}
