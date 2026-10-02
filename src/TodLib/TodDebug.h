#ifndef __TODDEBUG_H__
#define __TODDEBUG_H__

#include <Windows.h>
#include "StackWalk.h"
#include "../SexyAppFramework/PerfTimer.h"
#include "PvzDebug.h"

class TodHesitationBracket
{
public:
	char					mMessage[256];
	int						mBracketStartTime;

public:
	TodHesitationBracket(const char* theFormat, ...) { ; }
	~TodHesitationBracket() { ; }

	inline void				EndBracket() { ; }
};

class HesitationBuffer
{
public:
	int						mCurrentPos;
	Sexy::PerfTimer			mTimer;
	int						mLastTrace;
	int						mIndent;
	bool					mRecording;
	char					mText[0x40000];

public:
	HesitationBuffer();
};
extern HesitationBuffer gHesitation;

class TodMemoryTrace
{
public: 
	TodMemoryTrace*			mNext;
	TodMemoryTrace*			mPrev;
	StackWalkMiniStack		mMiniStack;
	int						mBytes;
};

class TodMemoryAppData
{
public:
	bool					mInitialized;
	RTL_CRITICAL_SECTION	mCriticalSection;
	TodMemoryTrace*			mTraceHead;
};
extern TodMemoryAppData* gMemoryAppData;

extern char gLogFileName[MAX_PATH];
extern char gDebugDataFolder[MAX_PATH];

void						TodLog(const char* theFormat, ...);
void						TodLogString(const char* theMsg);
void						TodTrace(const char* theFormat, ...);
DECLSPEC_NORETURN void		TodTraceMemory();
void						TodTraceAndLog(const char* theFormat, ...);
void						TodTraceWithoutSpamming(const char* theFormat, ...);
void						TodHesitationTrace(...);
void						TodGetExeDirectory(char* theExePath);
void						TodVsnprintfEnsureNewLine(char* aBuffer, int size, const char* theFormat, va_list theArgList);
void						TodCrashScreenshot(const char* theName);
void						TodReportError(LPEXCEPTION_POINTERS exceptioninfo, const char* theMessage);
void						TodAssertFailed(const char* theCondition, const char* theFile, int theLine, const char* theMsg = "", ...);
void						TodErrorMessageBox(const char* theMessage, const char* theTitle);
long __stdcall				TodUnhandledExceptionFilter(LPEXCEPTION_POINTERS exceptioninfo);

void*						TodMalloc(int theSize);
void						TodFree(void* theBlock);
void						TodAssertInitForApp();

extern void					(*gBetaSubmitFunc)();

// -------------------------------------------------------------------------------------------------
// TOD_ASSERT
//
// Debug builds keep the original behaviour: TodAssertFailed() reports through the
// PopCap machinery (stack walk, memory trace) and then exits, which is fine when a
// debugger is attached.
//
// Release builds are the problem. The shipped configuration is Release|Win32 with
// NDEBUG, and until now TOD_ASSERT compiled to literally nothing there - so every
// invariant in the codebase (entity pool bounds, wave indices, non-null images) was
// checked in Debug and silently ignored in the build people actually run. A mistake
// then surfaced only as an access violation with no context.
//
// Release now routes through PvzDebug instead: the failure is written to
// pvzdebug.log with file/line/condition, and by default execution CONTINUES so a
// playtest is not cut short. Set PVZ_ASSERT_ACTION=1 (break) or 2 (abort) to make
// Release asserts fatal again.
// -------------------------------------------------------------------------------------------------

#ifdef _DEBUG

	#define TOD_ASSERT(condition, ...) {										\
		if (!bool(condition))													\
		{																		\
			PvzAssertReport(""#condition, __FILE__, __LINE__, "" __VA_ARGS__);	\
			TodAssertFailed(""#condition, __FILE__, __LINE__, ##__VA_ARGS__);	\
			if (IsDebuggerPresent())											\
			{																	\
				__debugbreak();													\
			}																	\
			TodTraceMemory();													\
		}																		\
	}

#else

	#define TOD_ASSERT(condition, ...)											\
		do {																	\
			if (!bool(condition))												\
			{																	\
				if (PvzAssertReport(""#condition, __FILE__, __LINE__,			\
						"" __VA_ARGS__) == PVZ_ASSERT_BREAK)					\
				{																\
					if (IsDebuggerPresent())									\
						__debugbreak();											\
				}																\
			}																	\
		} while (false)

#endif

#endif
