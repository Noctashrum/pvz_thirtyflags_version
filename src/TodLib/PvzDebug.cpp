// PvzDebug implementation. See PvzDebug.h for the rationale and usage.
//
// ASCII ONLY - the rest of src/ is GBK (code page 936).

#include "PvzDebug.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// --------------------------------------------------------------------------------------------------
// Tunables
// --------------------------------------------------------------------------------------------------
#define PVZ_MAX_LINE			2048	// one formatted log line
#define PVZ_RECENT_COUNT		192		// ring buffer entries kept for crash reports
#define PVZ_RECENT_WIDTH		256		// bytes per ring buffer entry
#define PVZ_ROTATE_BYTES		8388608	// 8 MB, then rotate
#define PVZ_MAX_ASSERTS			20000u	// stop logging asserts after this many
#define PVZ_ASSERT_REPEAT_EVERY	100u	// throttling for an assert that fires every frame

// --------------------------------------------------------------------------------------------------
// State
//
// A simple interlocked spinlock is used instead of CRITICAL_SECTION on purpose:
// it needs no initialization, so logging from another translation unit's static
// constructor (i.e. before PvzDebugInit could possibly run) stays safe.
// --------------------------------------------------------------------------------------------------
static volatile LONG		gPvzLock = 0;
static FILE*				gPvzFile = nullptr;
static int					gPvzReady = 0;
static int					gPvzInCrash = 0;

static int					gPvzLevel = PVZ_LOG_INFO;
static unsigned int			gPvzChannels = PVZ_CH_ALL;
static int					gPvzAssertAction = PVZ_ASSERT_CONTINUE;

static unsigned int			gPvzAssertCount = 0;
static unsigned int			gPvzErrorCount = 0;
static unsigned int			gPvzLogCount = 0;
static unsigned int			gPvzBytesWritten = 0;

static char					gPvzRecent[PVZ_RECENT_COUNT][PVZ_RECENT_WIDTH];
static int					gPvzRecentHead = 0;
static int					gPvzRecentUsed = 0;

// Sized MAX_PATH + slack: "%s.old" / "%s.dmp" are built from these, and
// sprintf_s() calls the invalid-parameter handler (i.e. kills the process) if the
// output does not fit - which would be especially bad inside the crash handler.
static char					gPvzLogPath[MAX_PATH + 16];
static char					gPvzRotatePath[MAX_PATH + 16];

static PvzStateDumpFn		gPvzStateDump = nullptr;
static void*				gPvzStateDumpCtx = nullptr;
static int					gPvzCrashHandlerInstalled = 0;

// --------------------------------------------------------------------------------------------------
// Locking
// --------------------------------------------------------------------------------------------------
static void PvzLockAcquire(void)
{
	if (gPvzInCrash)
		return; // never block inside the crash path

	while (InterlockedCompareExchange(&gPvzLock, 1L, 0L) != 0L)
		Sleep(0);
}

static void PvzLockRelease(void)
{
	if (gPvzInCrash)
		return;

	InterlockedExchange(&gPvzLock, 0L);
}

// --------------------------------------------------------------------------------------------------
// Small helpers
// --------------------------------------------------------------------------------------------------
static char PvzLevelChar(int theLevel)
{
	switch (theLevel)
	{
	case PVZ_LOG_ERROR:		return 'E';
	case PVZ_LOG_WARN:		return 'W';
	case PVZ_LOG_INFO:		return 'I';
	case PVZ_LOG_VERBOSE:	return 'V';
	default:				return '?';
	}
}

// Compact 4-char tag for a single channel; hex when several bits are set.
static void PvzChannelTag(unsigned int theChannel, char* theOut, int theOutSize)
{
	static const struct { unsigned int mBit; const char* mName; } aNames[] =
	{
		{ PVZ_CH_CORE,			"core" },
		{ PVZ_CH_BOARD,			"brd " },
		{ PVZ_CH_PLANT,			"plnt" },
		{ PVZ_CH_ZOMBIE,		"zomb" },
		{ PVZ_CH_PROJECTILE,	"proj" },
		{ PVZ_CH_GRIDITEM,		"grid" },
		{ PVZ_CH_COIN,			"coin" },
		{ PVZ_CH_CHALLENGE,		"chal" },
		{ PVZ_CH_UI,			"ui  " },
		{ PVZ_CH_RESOURCE,		"res " },
		{ PVZ_CH_MEMORY,		"mem " },
		{ PVZ_CH_RENDER,		"rend" },
		{ PVZ_CH_SAVEGAME,		"save" },
		{ PVZ_CH_TF,			"tf  " },
		{ PVZ_CH_USER,			"user" }
	};

	if (theChannel == 0u)
	{
		strcpy_s(theOut, theOutSize, "----");
		return;
	}

	int aBitCount = 0;
	unsigned int aFound = 0;
	const char* aFoundName = "????";
	for (int i = 0; i < (int)(sizeof(aNames) / sizeof(aNames[0])); i++)
	{
		if (theChannel & aNames[i].mBit)
		{
			aBitCount++;
			aFound = aNames[i].mBit;
			aFoundName = aNames[i].mName;
		}
	}

	if (aBitCount == 1)
		strcpy_s(theOut, theOutSize, aFoundName);
	else
	{
		// Several channels at once (or PVZ_CH_ALL): show the mask.
		if (theChannel == PVZ_CH_ALL)
			strcpy_s(theOut, theOutSize, "all ");
		else
			sprintf_s(theOut, theOutSize, "%04X", (unsigned int)(theChannel & 0xFFFFu));
	}
	(void)aFound;
}

static const char* PvzBaseName(const char* thePath)
{
	if (thePath == nullptr)
		return "?";
	const char* aSlash = strrchr(thePath, '\\');
	const char* aSlash2 = strrchr(thePath, '/');
	if (aSlash2 != nullptr && (aSlash == nullptr || aSlash2 > aSlash))
		aSlash = aSlash2;
	return (aSlash != nullptr) ? aSlash + 1 : thePath;
}

// --------------------------------------------------------------------------------------------------
// Output
// --------------------------------------------------------------------------------------------------
// Set while the ring buffer is being replayed, so replaying it does not
// overwrite the very entries still waiting to be printed.
static int	gPvzNoRemember = 0;

static void PvzRemember(const char* theText)
{
	if (gPvzNoRemember || theText == nullptr)
		return;

	// Deliberately not strcpy_s(): that calls MSVC's invalid-parameter handler when
	// the source does not fit, which would terminate the process - from inside the
	// crash handler, of all places. Bounded copy instead, and keep the entry
	// newline-terminated so the crash dump stays readable when it was truncated.
	int aSlot = gPvzRecentHead;

	size_t aLen = strlen(theText);
	if (aLen >= (size_t)PVZ_RECENT_WIDTH)
		aLen = PVZ_RECENT_WIDTH - 1;
	memcpy(gPvzRecent[aSlot], theText, aLen);
	gPvzRecent[aSlot][aLen] = '\0';

	if (aLen > 0 && gPvzRecent[aSlot][aLen - 1] != '\n')
	{
		if (aLen < (size_t)PVZ_RECENT_WIDTH - 1)
		{
			gPvzRecent[aSlot][aLen] = '\n';
			gPvzRecent[aSlot][aLen + 1] = '\0';
		}
		else
		{
			gPvzRecent[aSlot][PVZ_RECENT_WIDTH - 2] = '\n';
		}
	}

	gPvzRecentHead = (gPvzRecentHead + 1) % PVZ_RECENT_COUNT;
	if (gPvzRecentUsed < PVZ_RECENT_COUNT)
		gPvzRecentUsed++;
}

static void PvzRotateIfNeeded(void)
{
	if (gPvzFile == nullptr || gPvzBytesWritten < PVZ_ROTATE_BYTES)
		return;

	fclose(gPvzFile);
	gPvzFile = nullptr;

	// Keep one generation of history.
	remove(gPvzRotatePath);
	rename(gPvzLogPath, gPvzRotatePath);

	gPvzFile = fopen(gPvzLogPath, "w");
	gPvzBytesWritten = 0;
	if (gPvzFile != nullptr)
		fprintf(gPvzFile, "# rotated, previous log kept as %s\n", gPvzRotatePath);
}

// Writes one already-terminated line to every sink. Caller holds the lock (or is crashing).
static void PvzEmit(const char* theText, int theLevel)
{
	PvzRemember(theText);

	if (gPvzFile != nullptr)
	{
		fputs(theText, gPvzFile);
		gPvzBytesWritten += (unsigned int)strlen(theText);

		// Errors must survive a hard crash, so they go to disk immediately.
		// Everything else is flushed periodically to keep the cost down.
		if (theLevel <= PVZ_LOG_WARN || (gPvzLogCount & 63u) == 0u)
			fflush(gPvzFile);

		PvzRotateIfNeeded();
	}
	else
	{
		// Not initialized (or init failed) - at least get it into a debugger.
		OutputDebugStringA(theText);
	}
}

// --------------------------------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------------------------------
static int PvzEnvInt(const char* theName, int theDefault)
{
	char aBuf[64];
	DWORD aLen = GetEnvironmentVariableA(theName, aBuf, sizeof(aBuf));
	if (aLen == 0 || aLen >= sizeof(aBuf))
		return theDefault;
	return atoi(aBuf);
}

static unsigned int PvzEnvHex(const char* theName, unsigned int theDefault)
{
	char aBuf[64];
	DWORD aLen = GetEnvironmentVariableA(theName, aBuf, sizeof(aBuf));
	if (aLen == 0 || aLen >= sizeof(aBuf))
		return theDefault;
	return (unsigned int)strtoul(aBuf, nullptr, 16);
}

// Reads "key=value" lines from pvzdebug.ini if it exists. Env vars win over the file.
static void PvzReadIni(void)
{
	FILE* aFile = fopen("pvzdebug.ini", "r");
	if (aFile == nullptr)
		return;

	char aLine[256];
	while (fgets(aLine, sizeof(aLine), aFile) != nullptr)
	{
		// Skip blanks and comments.
		char* aFirst = aLine;
		while (*aFirst == ' ' || *aFirst == '\t')
			aFirst++;
		if (*aFirst == '\0' || *aFirst == '#' || *aFirst == ';' || *aFirst == '[')
			continue;

		char* aEq = strchr(aFirst, '=');
		if (aEq == nullptr)
			continue;
		*aEq = '\0';

		char* aKey = aFirst;
		char* aVal = aEq + 1;

		// "level = 3" must parse, so trim both sides - otherwise the key keeps a
		// trailing space and never matches.
		while (*aVal == ' ' || *aVal == '\t')
			aVal++;

		char* aEnd = aKey + strlen(aKey) - 1;
		while (aEnd >= aKey && (*aEnd == ' ' || *aEnd == '\t'))
			*(aEnd--) = '\0';

		aEnd = aVal + strlen(aVal) - 1;
		while (aEnd >= aVal && (*aEnd == ' ' || *aEnd == '\t' || *aEnd == '\r' || *aEnd == '\n'))
			*(aEnd--) = '\0';

		if (_stricmp(aKey, "level") == 0)
		{
			if (GetEnvironmentVariableA("PVZ_LOG_LEVEL", nullptr, 0) == 0)
				gPvzLevel = atoi(aVal);
		}
		else if (_stricmp(aKey, "channels") == 0)
		{
			if (GetEnvironmentVariableA("PVZ_LOG_CHANNELS", nullptr, 0) == 0)
				gPvzChannels = (unsigned int)strtoul(aVal, nullptr, 16);
		}
		else if (_stricmp(aKey, "action") == 0)
		{
			if (GetEnvironmentVariableA("PVZ_ASSERT_ACTION", nullptr, 0) == 0)
			{
				if (_stricmp(aVal, "break") == 0)		gPvzAssertAction = PVZ_ASSERT_BREAK;
				else if (_stricmp(aVal, "abort") == 0)	gPvzAssertAction = PVZ_ASSERT_ABORT;
				else									gPvzAssertAction = atoi(aVal);
			}
		}
	}
	fclose(aFile);
}

// --------------------------------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------------------------------
void PvzDebugInit(const char* theLogPath)
{
	if (gPvzReady)
		return;

	// Default log location: beside the executable, not the current directory,
	// because the game is often launched with a different working directory.
	if (theLogPath != nullptr && *theLogPath != '\0')
	{
		size_t aLen = strlen(theLogPath);
		if (aLen >= sizeof(gPvzLogPath))
			aLen = sizeof(gPvzLogPath) - 1;
		memcpy(gPvzLogPath, theLogPath, aLen);
		gPvzLogPath[aLen] = '\0';
	}
	else
	{
		char aModule[MAX_PATH];
		DWORD aLen = GetModuleFileNameA(nullptr, aModule, MAX_PATH);
		if (aLen == 0 || aLen >= MAX_PATH)
			strcpy_s(gPvzLogPath, MAX_PATH, "pvzdebug.log");
		else
		{
			char* aSlash = strrchr(aModule, '\\');
			if (aSlash != nullptr)
				*(aSlash + 1) = '\0';
			sprintf_s(gPvzLogPath, sizeof(gPvzLogPath), "%spvzdebug.log", aModule);
		}
	}
	sprintf_s(gPvzRotatePath, sizeof(gPvzRotatePath), "%s.old", gPvzLogPath);

	PvzReadIni();

	gPvzLevel = PvzEnvInt("PVZ_LOG_LEVEL", gPvzLevel);
	if (gPvzLevel < PVZ_LOG_OFF) gPvzLevel = PVZ_LOG_OFF;
	if (gPvzLevel > PVZ_LOG_VERBOSE) gPvzLevel = PVZ_LOG_VERBOSE;

	gPvzChannels = PvzEnvHex("PVZ_LOG_CHANNELS", gPvzChannels);
	gPvzAssertAction = PvzEnvInt("PVZ_ASSERT_ACTION", gPvzAssertAction);

	gPvzFile = fopen(gPvzLogPath, "a");
	gPvzBytesWritten = 0;
	if (gPvzFile != nullptr)
	{
		// Remember where we are so the rotate limit is meaningful across restarts.
		fseek(gPvzFile, 0, SEEK_END);
		long aSize = ftell(gPvzFile);
		if (aSize > 0)
			gPvzBytesWritten = (unsigned int)aSize;
	}

	gPvzReady = 1;

	char aTime[64];
	{
		SYSTEMTIME aNow;
		GetLocalTime(&aNow);
		sprintf_s(aTime, sizeof(aTime), "%04d-%02d-%02d %02d:%02d:%02d",
			aNow.wYear, aNow.wMonth, aNow.wDay, aNow.wHour, aNow.wMinute, aNow.wSecond);
	}

	PvzLockAcquire();
	{
		char aLine[PVZ_MAX_LINE];
		sprintf_s(aLine, sizeof(aLine),
			"\n===========================================================================\n"
			"# PvzDebug session start %s\n"
			"# log=%s level=%d channels=0x%08X assertAction=%d\n"
			"===========================================================================\n",
			aTime, gPvzLogPath, gPvzLevel, gPvzChannels, gPvzAssertAction);
		PvzEmit(aLine, PVZ_LOG_ERROR);
	}
	PvzLockRelease();

	PvzDebugInstallCrashHandler();
}

void PvzDebugShutdown(void)
{
	if (!gPvzReady)
		return;

	PvzLockAcquire();
	{
		char aLine[PVZ_MAX_LINE];
		sprintf_s(aLine, sizeof(aLine),
			"# session end asserts=%u errors=%u lines=%u\n",
			gPvzAssertCount, gPvzErrorCount, gPvzLogCount);
		PvzEmit(aLine, PVZ_LOG_ERROR);
	}
	PvzLockRelease();

	if (gPvzFile != nullptr)
	{
		fflush(gPvzFile);
		fclose(gPvzFile);
		gPvzFile = nullptr;
	}
	gPvzReady = 0;
}

int PvzDebugIsReady(void)
{
	return gPvzReady;
}

// --------------------------------------------------------------------------------------------------
// Configuration accessors
// --------------------------------------------------------------------------------------------------
void PvzDebugSetLevel(int theLevel)					{ gPvzLevel = theLevel; }
void PvzDebugSetChannels(unsigned int theChannels)	{ gPvzChannels = theChannels; }
void PvzDebugSetAssertAction(int theAction)			{ gPvzAssertAction = theAction; }
int PvzDebugGetLevel(void)							{ return gPvzLevel; }
unsigned int PvzDebugGetChannels(void)				{ return gPvzChannels; }
int PvzDebugGetAssertAction(void)					{ return gPvzAssertAction; }

void PvzDebugEnableChannel(unsigned int theChannel, int theEnable)
{
	if (theEnable)
		gPvzChannels |= theChannel;
	else
		gPvzChannels &= ~theChannel;
}

unsigned int PvzDebugAssertCount(void)	{ return gPvzAssertCount; }
unsigned int PvzDebugErrorCount(void)	{ return gPvzErrorCount; }
unsigned int PvzDebugLogCount(void)		{ return gPvzLogCount; }

// --------------------------------------------------------------------------------------------------
// Logging
// --------------------------------------------------------------------------------------------------
int PvzLogIsEnabled(int theLevel, unsigned int theChannel)
{
	if (!PVZ_DEBUG_ENABLED)
		return 0;
	if (theLevel > gPvzLevel)
		return 0;
	if (theChannel == PVZ_CH_NONE)
		return 1;
	return (gPvzChannels & theChannel) != 0u;
}

void PvzLogRaw(const char* theText)
{
	if (theText == nullptr)
		return;

	PvzLockAcquire();
	PvzEmit(theText, PVZ_LOG_WARN);
	PvzLockRelease();
}

void PvzLogEx(int theLevel, unsigned int theChannel, const char* theFile,
	int theLine, const char* theFormat, ...)
{
	if (theFormat == nullptr)
		return;

	SYSTEMTIME aNow;
	GetLocalTime(&aNow);

	char aTag[16];
	PvzChannelTag(theChannel, aTag, sizeof(aTag));

	char aMsg[PVZ_MAX_LINE];
	va_list aArgs;
	va_start(aArgs, theFormat);
	vsnprintf(aMsg, sizeof(aMsg), theFormat, aArgs);
	va_end(aArgs);
	aMsg[sizeof(aMsg) - 1] = '\0';

	char aLine[PVZ_MAX_LINE + 256];
	sprintf_s(aLine, sizeof(aLine), "%02d:%02d:%02d.%03d [%c] [%s] %5u %s(%d): %s\n",
		aNow.wHour, aNow.wMinute, aNow.wSecond, aNow.wMilliseconds,
		PvzLevelChar(theLevel), aTag, (unsigned int)GetCurrentThreadId(),
		PvzBaseName(theFile), theLine, aMsg);

	PvzLockAcquire();
	gPvzLogCount++;
	if (theLevel <= PVZ_LOG_ERROR)
		gPvzErrorCount++;
	PvzEmit(aLine, theLevel);
	PvzLockRelease();
}

// --------------------------------------------------------------------------------------------------
// Assertions
// --------------------------------------------------------------------------------------------------
int PvzAssertReport(const char* theCondition, const char* theFile, int theLine,
	const char* theFormat, ...)
{
	gPvzAssertCount++;

	// An assert inside a hot loop can fire thousands of times a second. Write the
	// first one in full, then only a periodic reminder so the log stays readable.
	static const char*	sLastFile = nullptr;
	static int			sLastLine = 0;
	static unsigned int	sRepeat = 0;

	int aIsRepeat = (sLastFile != nullptr && theFile != nullptr && sLastLine == theLine &&
		strcmp(sLastFile, theFile) == 0);
	if (aIsRepeat)
		sRepeat++;
	else
	{
		sRepeat = 0;
		sLastFile = theFile;
		sLastLine = theLine;
	}

	// Always count, but stop writing once the log has clearly served its purpose.
	int aShouldWrite = (gPvzAssertCount <= PVZ_MAX_ASSERTS) &&
		(!aIsRepeat || (sRepeat % PVZ_ASSERT_REPEAT_EVERY) == 0u);

	if (aShouldWrite)
	{
		char aMsg[PVZ_MAX_LINE];
		aMsg[0] = '\0';
		if (theFormat != nullptr)
		{
			va_list aArgs;
			va_start(aArgs, theFormat);
			vsnprintf(aMsg, sizeof(aMsg), theFormat, aArgs);
			va_end(aArgs);
			aMsg[sizeof(aMsg) - 1] = '\0';
		}

		SYSTEMTIME aNow;
		GetLocalTime(&aNow);

		char aLine[PVZ_MAX_LINE + 512];
		sprintf_s(aLine, sizeof(aLine),
			"%02d:%02d:%02d.%03d [E] [mem ] %5u ASSERT %s(%d)\n"
			"      condition: %s\n"
			"      %s%s\n"
			"      (total asserts: %u%s)\n",
			aNow.wHour, aNow.wMinute, aNow.wSecond, aNow.wMilliseconds,
			(unsigned int)GetCurrentThreadId(), PvzBaseName(theFile), theLine,
			(theCondition != nullptr) ? theCondition : "(none)",
			(aMsg[0] != '\0') ? "detail:    " : "", aMsg,
			gPvzAssertCount,
			aIsRepeat ? ", REPEATED" : "");

		PvzLockAcquire();
		gPvzLogCount++;
		gPvzErrorCount++;
		PvzEmit(aLine, PVZ_LOG_ERROR);
		PvzLockRelease();
	}

	if (gPvzAssertCount == PVZ_MAX_ASSERTS)
	{
		PvzLogRaw("# assert limit reached, further assertions are counted but not written\n");
	}

	return gPvzAssertAction;
}

int PvzFailReport(const char* theFile, int theLine, const char* theFormat, ...)
{
	gPvzAssertCount++;

	char aMsg[PVZ_MAX_LINE];
	aMsg[0] = '\0';
	if (theFormat != nullptr)
	{
		va_list aArgs;
		va_start(aArgs, theFormat);
		vsnprintf(aMsg, sizeof(aMsg), theFormat, aArgs);
		va_end(aArgs);
		aMsg[sizeof(aMsg) - 1] = '\0';
	}

	SYSTEMTIME aNow;
	GetLocalTime(&aNow);

	char aLine[PVZ_MAX_LINE + 512];
	sprintf_s(aLine, sizeof(aLine),
		"%02d:%02d:%02d.%03d [E] [mem ] %5u FAIL %s(%d): %s\n",
		aNow.wHour, aNow.wMinute, aNow.wSecond, aNow.wMilliseconds,
		(unsigned int)GetCurrentThreadId(), PvzBaseName(theFile), theLine, aMsg);

	PvzLockAcquire();
	gPvzLogCount++;
	gPvzErrorCount++;
	PvzEmit(aLine, PVZ_LOG_ERROR);
	PvzLockRelease();

	return gPvzAssertAction;
}

// --------------------------------------------------------------------------------------------------
// Ring buffer
// --------------------------------------------------------------------------------------------------
void PvzDebugDumpRecent(void)
{
	gPvzNoRemember = 1;

	PvzLogRaw("---- recent log (oldest first) ----\n");

	int aStart = (gPvzRecentUsed < PVZ_RECENT_COUNT) ? 0 : gPvzRecentHead;
	for (int i = 0; i < gPvzRecentUsed; i++)
	{
		int aSlot = (aStart + i) % PVZ_RECENT_COUNT;
		PvzLogRaw(gPvzRecent[aSlot]);
	}

	PvzLogRaw("---- end recent log ----\n");

	gPvzNoRemember = 0;
}

void PvzDebugFlush(void)
{
	if (gPvzFile != nullptr)
		fflush(gPvzFile);
}

// --------------------------------------------------------------------------------------------------
// Crash handling
// --------------------------------------------------------------------------------------------------
void PvzDebugSetStateDump(PvzStateDumpFn theFn, void* theContext)
{
	gPvzStateDump = theFn;
	gPvzStateDumpCtx = theContext;
}

static const char* PvzExceptionName(DWORD theCode)
{
	switch (theCode)
	{
	case 0xC0000005u:	return "ACCESS VIOLATION";
	case 0xC0000094u:	return "INTEGER DIVIDE BY ZERO";
	case 0xC0000095u:	return "INTEGER OVERFLOW";
	case 0xC00000FDu:	return "STACK OVERFLOW";
	case 0xC000008Eu:	return "FLT DIVIDE BY ZERO";
	case 0xC000008Fu:	return "FLT INEXACT RESULT";
	case 0xC0000090u:	return "FLT INVALID OPERATION";
	case 0xC0000091u:	return "FLT OVERFLOW";
	case 0xC0000092u:	return "FLT UNDERFLOW";
	case 0xC0000093u:	return "FLT DENORMAL OPERAND";
	case 0xC000001Du:	return "ILLEGAL INSTRUCTION";
	case 0xC0000025u:	return "NONCONTINUABLE EXCEPTION";
	case 0xC0000026u:	return "INVALID DISPOSITION";
	case 0xC000008Cu:	return "ARRAY BOUNDS EXCEEDED";
	case 0xC0000135u:	return "DLL NOT FOUND";
	case 0xC0000409u:	return "STACK BUFFER OVERRUN";
	case 0xC0000417u:	return "INVALID CRUNTIME PARAMETER";
	case 0xE06D7363u:	return "C++ EXCEPTION";
	default:			return "unknown";
	}
}

// Minimal dbghelp binding, declared locally so this file does not need
// <minidumpapiset.h> and cannot clash with StackWalk.h.
typedef BOOL (WINAPI* PvzMiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, DWORD, void*, void*, void*);

struct PvzMiniDumpExceptionInfo
{
	DWORD	mThreadId;
	void*	mExceptionPointers;
	int		mClientPointers;
};

static void PvzWriteMiniDump(struct _EXCEPTION_POINTERS* theInfo)
{
	HMODULE aDbgHelp = LoadLibraryA("dbghelp.dll");
	if (aDbgHelp == nullptr)
	{
		PvzLogRaw("# minidump skipped: dbghelp.dll not available\n");
		return;
	}

	PvzMiniDumpWriteDumpFn aWrite = (PvzMiniDumpWriteDumpFn)GetProcAddress(
		aDbgHelp, "MiniDumpWriteDump");
	if (aWrite == nullptr)
	{
		PvzLogRaw("# minidump skipped: MiniDumpWriteDump not found\n");
		return;
	}

	char aDumpPath[MAX_PATH + 16];
	sprintf_s(aDumpPath, sizeof(aDumpPath), "%s.dmp", gPvzLogPath);

	HANDLE aFile = CreateFileA(aDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (aFile == INVALID_HANDLE_VALUE)
	{
		PvzLogRaw("# minidump skipped: could not create dump file\n");
		return;
	}

	PvzMiniDumpExceptionInfo aExInfo;
	aExInfo.mThreadId = (unsigned int)GetCurrentThreadId();
	aExInfo.mExceptionPointers = (void*)theInfo;
	aExInfo.mClientPointers = 0;

	// 0 == MiniDumpNormal
	BOOL aOk = aWrite(GetCurrentProcess(), GetCurrentProcessId(), aFile, 0u,
		&aExInfo, nullptr, nullptr);

	CloseHandle(aFile);

	char aLine[PVZ_MAX_LINE];
	sprintf_s(aLine, sizeof(aLine), "# minidump %s: %s\n",
		aOk ? "written" : "FAILED", aDumpPath);
	PvzLogRaw(aLine);
}

static LONG WINAPI PvzCrashFilter(struct _EXCEPTION_POINTERS* theInfo)
{
	// A fault raised inside the handler itself must not recurse.
	if (gPvzInCrash)
		return EXCEPTION_EXECUTE_HANDLER;
	gPvzInCrash = 1;

	DWORD aCode = 0u;
	void* aAddress = nullptr;
	if (theInfo != nullptr && theInfo->ExceptionRecord != nullptr)
	{
		aCode = theInfo->ExceptionRecord->ExceptionCode;
		aAddress = theInfo->ExceptionRecord->ExceptionAddress;
	}

	char aLine[PVZ_MAX_LINE];

	PvzLogRaw("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

	sprintf_s(aLine, sizeof(aLine), "!!! UNHANDLED EXCEPTION 0x%08X (%s)\n",
		(unsigned int)aCode, PvzExceptionName(aCode));
	PvzLogRaw(aLine);

	sprintf_s(aLine, sizeof(aLine), "!!! fault address: %p   thread: %u\n",
		aAddress, (unsigned int)GetCurrentThreadId());
	PvzLogRaw(aLine);

	// Access violations: say whether it was a read or a write and at what address.
	if (theInfo != nullptr && theInfo->ExceptionRecord != nullptr &&
		(aCode == 0xC0000005u) && theInfo->ExceptionRecord->NumberParameters >= 2)
	{
		ULONG_PTR aOp = theInfo->ExceptionRecord->ExceptionInformation[0];
		ULONG_PTR aTarget = theInfo->ExceptionRecord->ExceptionInformation[1];
		sprintf_s(aLine, sizeof(aLine), "!!! %s at address %p\n",
			(aOp == 1u) ? "write" : ((aOp == 0u) ? "read" : "execute"), (void*)aTarget);
		PvzLogRaw(aLine);

		// A near-null target is nearly always a missing object or a stale handle.
		if (aTarget < 0x10000u)
			PvzLogRaw("!!! target is near NULL - likely a null pointer or stale object id\n");
	}

#if defined(_M_IX86)
	if (theInfo != nullptr && theInfo->ContextRecord != nullptr)
	{
		PCONTEXT aCtx = theInfo->ContextRecord;
		sprintf_s(aLine, sizeof(aLine),
			"!!! regs EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X\n",
			aCtx->Eax, aCtx->Ebx, aCtx->Ecx, aCtx->Edx, aCtx->Esi, aCtx->Edi);
		PvzLogRaw(aLine);
		sprintf_s(aLine, sizeof(aLine),
			"!!!      EIP=%08X ESP=%08X EBP=%08X\n",
			aCtx->Eip, aCtx->Esp, aCtx->Ebp);
		PvzLogRaw(aLine);
	}
#endif

	// What the game was doing when it died.
	if (gPvzStateDump != nullptr)
	{
		PvzLogRaw("---- game state at crash ----\n");
		__try
		{
			gPvzStateDump(gPvzStateDumpCtx);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			PvzLogRaw("# state dump itself faulted\n");
		}
		PvzLogRaw("---- end game state ----\n");
	}

	// The last messages before the fault: usually the most useful part.
	PvzDebugDumpRecent();

	PvzWriteMiniDump(theInfo);

	PvzDebugFlush();

	PvzLogRaw("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
	PvzDebugFlush();

	return EXCEPTION_EXECUTE_HANDLER;
}

void PvzDebugInstallCrashHandler(void)
{
	// Installed unconditionally rather than once: other subsystems in this codebase
	// install their own top-level filter, and whichever runs last wins. Re-installing
	// keeps PvzDebug's handler in charge.
	gPvzCrashHandlerInstalled = 1;
	SetUnhandledExceptionFilter(PvzCrashFilter);
}

// --------------------------------------------------------------------------------------------------
// Scope trace
// --------------------------------------------------------------------------------------------------
#if PVZ_DEBUG_ENABLED

PvzScopeTrace::PvzScopeTrace(unsigned int theChannel, const char* theFile, int theLine,
	const char* theFormat, ...)
{
	mChannel = theChannel;
	mFile = theFile;
	mLine = theLine;
	mStartMs = GetTickCount();
	mLabel[0] = '\0';

	if (theFormat != nullptr)
	{
		va_list aArgs;
		va_start(aArgs, theFormat);
		vsnprintf(mLabel, sizeof(mLabel), theFormat, aArgs);
		va_end(aArgs);
		mLabel[sizeof(mLabel) - 1] = '\0';
	}

	if (PvzLogIsEnabled(PVZ_LOG_VERBOSE, mChannel))
		PvzLogEx(PVZ_LOG_VERBOSE, mChannel, mFile, mLine, ">> enter %s", mLabel);
}

PvzScopeTrace::~PvzScopeTrace()
{
	DWORD anElapsed = GetTickCount() - mStartMs;
	if (PvzLogIsEnabled(PVZ_LOG_VERBOSE, mChannel))
		PvzLogEx(PVZ_LOG_VERBOSE, mChannel, mFile, mLine, "<< exit  %s (%u ms)",
			mLabel, anElapsed);
}

#endif // PVZ_DEBUG_ENABLED
