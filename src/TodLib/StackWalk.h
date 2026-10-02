#ifndef __STACKWALK_H__
#define __STACKWALK_H__

#include <Windows.h>
#include <ImageHlp.h>
#include <minidumpapiset.h>

#define MINISTACK_MAX_FRAMES 12

class StackWalkMiniStack
{
public:
	int									mFrameCount;
	DWORD								mFrame[MINISTACK_MAX_FRAMES];
};

typedef BOOL(*PMINI_DUMP_WRITE_DUMP_ROUTINE)(
	HANDLE								hProcess, 
	DWORD								ProcessID, 
	HANDLE								hFile, 
	MINIDUMP_TYPE						DumpType, 
	PMINIDUMP_EXCEPTION_INFORMATION		ExceptionParam, 
	PMINIDUMP_USER_STREAM_INFORMATION	UserStreamParam, 
	PMINIDUMP_CALLBACK_INFORMATION		CallbackParam);

typedef BOOL(*PSTACK_WALK_ROUTINE)(
	DWORD								MachineType,
	HANDLE								hProcess,
	HANDLE								hThread,
	LPSTACKFRAME						StackFrame,
	PVOID								ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE		ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE		FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE			GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE			TranslateAddress);

typedef BOOL(*PINITIALIZE_ROUTINE)(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);
typedef DWORD(*PSET_OPTIONS_ROUTINE)(DWORD SymOptions);
typedef DWORD(*PUNDECORATE_SYMBOL_NAME_ROUTINE)(PCSTR name, PSTR outputString, DWORD maxStringLength, DWORD flags);
typedef BOOL(*PCLEAN_UP_ROUTINE)(HANDLE hProcess);
typedef BOOL(*PGET_SYM_FROM_ADDR_ROUTINE)(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_SYMBOL Symbol);
typedef BOOL(*PGET_LINE_FROM_ADDR_ROUTINE)(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE Line);

extern PGET_LINE_FROM_ADDR_ROUTINE		gSymGetLineFromAddr;
extern PGET_SYM_FROM_ADDR_ROUTINE		gSymGetSymFromAddr;
extern PGET_MODULE_BASE_ROUTINE			gSymGetModuleBase;
extern PFUNCTION_TABLE_ACCESS_ROUTINE	gSymFunctionTableAccess;
extern PSTACK_WALK_ROUTINE				gStackWalk;
extern PCLEAN_UP_ROUTINE				gSymCleanUp;
extern PUNDECORATE_SYMBOL_NAME_ROUTINE	gUnDecorateSymbolName;
extern PSET_OPTIONS_ROUTINE				gSymSetOptions;
extern PINITIALIZE_ROUTINE				gSymInitialize;
extern PMINI_DUMP_WRITE_DUMP_ROUTINE	gMiniDumpWriteDump;

extern HINSTANCE						gDbgHelpModule;

bool									StackWalkInitialize();
void									StackWalkShowMiniStack(StackWalkMiniStack* theMiniStack, bool theLogInsteadOfTrace, bool theUseExcelFormat);
void									StackWalkLogExceptionStack(LPEXCEPTION_POINTERS theExceptionInfo);
void									MiniDump(LPEXCEPTION_POINTERS pExceptionInfo, const char* theName);

#endif
