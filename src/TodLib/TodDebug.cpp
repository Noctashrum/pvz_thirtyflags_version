#include <time.h>
#include "TodDebug.h"
#include "TodCommon.h"
#include "../ImageLib/ImageLib.h"
#include "../SexyAppFramework/Debug.h"
#include "../SexyAppFramework/DDImage.h"
#include "../SexyAppFramework/SEHCatcher.h"
#include "../SexyAppFramework/DDInterface.h"
#include "../SexyAppFramework/SexyAppBase.h"

using namespace Sexy;

HesitationBuffer gHesitation;
TodMemoryAppData* gMemoryAppData = nullptr;

char gLogFileName[MAX_PATH];
char gDebugDataFolder[MAX_PATH];

HesitationBuffer::HesitationBuffer() : 
	mCurrentPos(0), 
	mLastTrace(0), 
	mIndent(0), 
	mRecording(true), 
	mText("")
{
}

//0x514EA0
void TodErrorMessageBox(const char* theMessage, const char* theTitle)
{
	HWND hWnd = (gSexyAppBase && gSexyAppBase->mHWnd) ? gSexyAppBase->mHWnd : GetActiveWindow();
#ifdef _DEBUG
	TodTraceAndLog("%s.%s", theMessage, theTitle);
#endif
	MessageBoxA(hWnd, theMessage, theTitle, MB_ICONEXCLAMATION);
}

DECLSPEC_NORETURN void TodTraceMemory()
{
}

void* TodMalloc(int theSize)
{
	TOD_ASSERT(theSize < 10000000);

	if (theSize > 0)
	{
		return malloc(theSize);
	}
	return nullptr;
}

void TodFree(void* theBlock)
{
	if (theBlock != nullptr)
	{
		free(theBlock);
	}
}

void TodAssertFailed(const char* theCondition, const char* theFile, int theLine, const char* theMsg, ...)
{
	char aFormattedMsg[1024];
	va_list argList;
	va_start(argList, theMsg);
	TodVsnprintfEnsureNewLine(aFormattedMsg, sizeof(aFormattedMsg), theMsg, argList);
	va_end(argList);

	char aBuffer[1024];
	if (*theCondition != '\0')
	{
		TodSnprintf(aBuffer, sizeof(aBuffer), "\n%s(%d)\nassertion failed: '%s'\n%s\n", theFile, theLine, theCondition, aFormattedMsg);
	}
	else
	{
		TodSnprintf(aBuffer, sizeof(aBuffer), "\n%s(%d)\nassertion failed: %s\n", theFile, theLine, aFormattedMsg);
	}
	TodTrace("%s", aBuffer);

	if (!IsDebuggerPresent())
	{
		if (gInAssert)
		{
			TodLog("Assert during exception processing.\n");
			exit(0);
		}


		gInAssert = true;
		LPEXCEPTION_POINTERS exp;

		__try
		{
			RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, NULL, NULL, NULL);
		}
		__except (exp = GetExceptionInformation(), EXCEPTION_CONTINUE_EXECUTION)
		{
			TodReportError(exp, aFormattedMsg);
		}

		gInAssert = false;
		exit(0);
	}
}

void TodLog(const char* theFormat, ...)
{
	char aBuffer[1024];
	va_list argList;
	va_start(argList, theFormat);
	TodVsnprintfEnsureNewLine(aBuffer, sizeof(aBuffer), theFormat, argList);
	va_end(argList);

	TodLogString(aBuffer);
}

void TodLogString(const char* theMsg)
{
	// Mirror everything into pvzdebug.log. In Release builds gLogFileName is empty
	// (TodAssertInitForApp is compiled out), so without this the messages the game
	// already produces would go nowhere at all.
	PvzLogRaw(theMsg);

	FILE* f = fopen(gLogFileName, "a");
	if (f == nullptr)
	{
		OutputDebugStringA("Failed to open log file\n");
		return;
	}

	if (fwrite(theMsg, strlen(theMsg), 1, f) != 1)
	{
		OutputDebugStringA("Failed to write to log file\n");
	}

	fclose(f);
}

void TodTrace(const char* theFormat, ...)
{
	char aBuffer[1024];
	va_list argList;
	va_start(argList, theFormat);
	TodVsnprintfEnsureNewLine(aBuffer, sizeof(aBuffer), theFormat, argList);
	va_end(argList);

	OutputDebugStringA(aBuffer);
}

void TodHesitationTrace(...)
{
}

void TodTraceAndLog(const char* theFormat, ...)
{
	char aBuffer[1024];
	va_list argList;
	va_start(argList, theFormat);
	TodVsnprintfEnsureNewLine(aBuffer, sizeof(aBuffer), theFormat, argList);
	va_end(argList);

	OutputDebugStringA(aBuffer);
	TodLogString(aBuffer);
}

void TodTraceWithoutSpamming(const char* theFormat, ...)
{
	static __time64_t gLastTraceTime = 0i64;
	__time64_t aTime = _time64(nullptr);
	if (aTime < gLastTraceTime)
	{
		return;
	}

	gLastTraceTime = aTime;
	char aBuffer[1024];
	va_list argList;
	va_start(argList, theFormat);
	TodVsnprintfEnsureNewLine(aBuffer, sizeof(aBuffer), theFormat, argList);
	va_end(argList);

	OutputDebugStringA(aBuffer);
}

void TodGetExeDirectory(char* theExePath)
{
	char aFilepath[_MAX_PATH];
	GetModuleFileNameA(NULL, aFilepath, LENGTH(aFilepath));

	char aDrive[_MAX_DRIVE];
	char aDir[_MAX_DIR];
	char aName[_MAX_FNAME];
	char aExt[_MAX_EXT];
	_splitpath(aFilepath, aDrive, aDir, aName, aExt);
	_makepath(theExePath, aDrive, aDir, nullptr, nullptr);
}

void TodVsnprintfEnsureNewLine(char* aBuffer, int size, const char* theFormat, va_list theArgList)
{
	int aLength = TodVsnprintf(aBuffer, size, theFormat, theArgList);
	if (aBuffer[aLength - 1] != '\n')
	{
		if (aLength + 1 < size)
		{
			aBuffer[aLength] = '\n';
			aBuffer[aLength + 1] = '\0';
		}
		else
		{
			aBuffer[aLength - 1] = '\n';
		}
	}
}

void TodCrashScreenshot(const char* theName)
{
	if (!gSexyAppBase->mDDInterface || !gSexyAppBase->mDDInterface->mDrawSurface)
	{
		TodTraceAndLog("DirectX not loaded for crash screen shot");
		return;
	}

	std::string anImageName = StrFormat("%sCrashScreenShot_%s.jpg", gDebugDataFolder, theName);

	LPDIRECTDRAWSURFACE aSurface = gSexyAppBase->mDDInterface->mDrawSurface;

	gSexyAppBase->mDDInterface->mDrawSurface = nullptr;

	DDImage anImage(gSexyAppBase->mDDInterface);
	anImage.SetSurface(aSurface);
	anImage.GetBits();
	anImage.DeleteDDSurface();
	gSexyAppBase->mDDInterface->mDrawSurface = aSurface;

	if (anImage.mBits == nullptr)
	{
		TodTraceAndLog("Failed to capture crash screen shot");
		return;
	}

	ImageLib::Image aSaveImage;
	aSaveImage.mBits = anImage.mBits;
	aSaveImage.mWidth = anImage.mWidth;
	aSaveImage.mHeight = anImage.mHeight;
	ImageLib::WriteJPEGImage(anImageName, &aSaveImage);
	aSaveImage.mBits = nullptr;

	TodTraceAndLog("Saved crash screen shot '%s'.", anImageName.c_str());
}

void TodReportError(LPEXCEPTION_POINTERS exceptioninfo, const char* theMessage)
{
	char oldPath[MAX_PATH];
	GetCurrentDirectoryA(LENGTH(oldPath), oldPath);

	char exePath[MAX_PATH];
	TodGetExeDirectory(exePath);
	SetCurrentDirectoryA(exePath);
	TodTraceAndLog("\n");
	TodLog("\n");

	StackWalkLogExceptionStack(exceptioninfo);
	
	char aUniqueCrashName[MAX_PATH];
	sprintf(aUniqueCrashName, "%u", _time32(nullptr));
	MiniDump(exceptioninfo, aUniqueCrashName);
	TodCrashScreenshot(aUniqueCrashName);

	SetCurrentDirectoryA(gDebugDataFolder);
	__try
	{
		Sexy::SEHCatcher::UnhandledExceptionFilter(exceptioninfo);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		TodLog("\n");
	}
	SetCurrentDirectoryA(oldPath);

	TodTraceAndLog("Error Message:\n%s\n\n", theMessage);
	TodTraceAndLog("User Text: \n%s\n\n", Sexy::SEHCatcher::mUserText);
	TodTraceAndLog("Error Title:\n%s\n\n", Sexy::SEHCatcher::mErrorTitle);
	TodTraceAndLog("Error Text:\n%s\n\n", Sexy::SEHCatcher::mErrorText);
	TodTraceAndLog("**************************************************************\n\n");

	if (gBetaSubmitFunc)
	{
		gBetaSubmitFunc();
	}
}

long __stdcall TodUnhandledExceptionFilter(LPEXCEPTION_POINTERS exceptioninfo)
{
	if (gInAssert)
	{
		TodLog("Exception during exception processing");
	}
	else
	{
		gInAssert = true;
		TodLog("\nUnhandled Exception\n");
		TodReportError(exceptioninfo, "Unhandled Exception");
		gInAssert = false;
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void (*gBetaSubmitFunc)() = nullptr;

void TodAssertInitForApp()
{
	MkDir(GetAppDataFolder() + "userdata");
	std::string aRelativeUserPath = GetAppDataFolder() + "userdata\\";
	strcpy(gDebugDataFolder, GetFullPath(aRelativeUserPath).c_str());
	strcpy(gLogFileName, gDebugDataFolder);
	strcpy(gLogFileName + strlen(gLogFileName), "log.txt");
	TOD_ASSERT(strlen(gLogFileName) < MAX_PATH);

	__time64_t aclock = _time64(nullptr);
	TodLog("Started %s\n", asctime(_localtime64(&aclock)));

	SetUnhandledExceptionFilter(TodUnhandledExceptionFilter);
}
