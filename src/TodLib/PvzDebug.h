#ifndef __PVZDEBUG_H__
#define __PVZDEBUG_H__

// ==================================================================================================
// PvzDebug - logging + assertion layer for the LawnProject rebuild.
//
// WHY THIS EXISTS
// ---------------
// The configuration actually shipped is Release|Win32 with NDEBUG. In that configuration
// TodDebug.h compiles TOD_ASSERT() down to nothing and TodAssertInitForApp() is never
// called, so the game writes no diagnostics at all. Every invariant the original authors
// left in the code (entity pool bounds, array indices, non-null images, wave limits) is
// silently discarded, and the only post-mortem evidence is whatever the OS leaves behind.
//
// That is the main reason modifications here are slow: a mistake surfaces as an access
// violation with no context, and the only way to investigate is to guess, rebuild and
// re-run. This module restores those checks in ALL configurations and records the
// results to disk, so a failure can be located by reading a file instead of by bisecting.
//
// DESIGN NOTES
// ------------
//  * Default policy is "report and continue". A bad invariant should not abort a
//    playtest; it should be written down. Switch to break/abort when chasing one.
//  * Logging is leveled AND channeled, so you can turn on just the subsystem you are
//    editing (zombies, grid items, the ThirtyFlags mod) without drowning in noise.
//  * A ring buffer keeps the last N messages in memory. On a crash those are flushed
//    first, so you get the moments leading up to the fault instead of only the fault.
//  * Nothing here throws, allocates through the game heap, or depends on init order
//    beyond PvzDebugInit(). Logging before init degrades to OutputDebugString.
//
// ENCODING
// --------
// ASCII ONLY, deliberately. The rest of src/ is GBK (code page 936) and the project
// builds MultiByte; dropping a UTF-8 file into a GBK translation unit corrupts the
// Chinese comments in that unit. Keep this file and PvzDebug.cpp free of non-ASCII.
// ==================================================================================================

#include <Windows.h>

// Master switch. Default is ON in every configuration, including Release - that is the
// point of this module. Define PVZ_DEBUG_ENABLED=0 on the compiler command line to
// compile the whole thing out (all macros become no-ops, no cost at runtime).
#ifndef PVZ_DEBUG_ENABLED
#	define PVZ_DEBUG_ENABLED 1
#endif

// --------------------------------------------------------------------------------------------------
// Verbosity. A message is written when its level <= the current level.
// --------------------------------------------------------------------------------------------------
enum PvzLogLevel
{
	PVZ_LOG_OFF		= 0,	// nothing
	PVZ_LOG_ERROR	= 1,	// invariant broken, data loss, crash-adjacent
	PVZ_LOG_WARN	= 2,	// suspicious but recoverable
	PVZ_LOG_INFO	= 3,	// lifecycle: level start, wave spawn, phase change
	PVZ_LOG_VERBOSE	= 4		// per-frame / per-entity spam
};

// --------------------------------------------------------------------------------------------------
// Channels. Filter what you are not working on.
// --------------------------------------------------------------------------------------------------
enum PvzLogChannel
{
	PVZ_CH_NONE			= 0x00000000u,
	PVZ_CH_CORE			= 0x00000001u,	// startup, shutdown, mode switches
	PVZ_CH_BOARD		= 0x00000002u,	// Board update / wave logic
	PVZ_CH_PLANT		= 0x00000004u,
	PVZ_CH_ZOMBIE		= 0x00000008u,
	PVZ_CH_PROJECTILE	= 0x00000010u,
	PVZ_CH_GRIDITEM		= 0x00000020u,
	PVZ_CH_COIN			= 0x00000040u,
	PVZ_CH_CHALLENGE	= 0x00000080u,
	PVZ_CH_UI			= 0x00000100u,
	PVZ_CH_RESOURCE		= 0x00000200u,	// image / reanim / sound loading
	PVZ_CH_MEMORY		= 0x00000400u,	// DataArray alloc/free/full  <-- watch this one
	PVZ_CH_RENDER		= 0x00000800u,
	PVZ_CH_SAVEGAME		= 0x00001000u,
	PVZ_CH_TF			= 0x00002000u,	// ThirtyFlags mod
	PVZ_CH_USER			= 0x00004000u,	// scratch channel for whatever you are debugging
	PVZ_CH_ALL			= 0xFFFFFFFFu
};

// --------------------------------------------------------------------------------------------------
// What to do after an assertion has been recorded.
// --------------------------------------------------------------------------------------------------
enum PvzAssertAction
{
	PVZ_ASSERT_CONTINUE	= 0,	// log it and keep running (default)
	PVZ_ASSERT_BREAK	= 1,	// break into the debugger if one is attached
	PVZ_ASSERT_ABORT	= 2		// terminate the process immediately
};

#if defined(__cplusplus)
extern "C" {
#endif

// --------------------------------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------------------------------

// Opens the log file and reads configuration. Safe to call more than once.
// theLogPath may be NULL to use the default ("pvzdebug.log" beside the exe).
// Call as early as possible in startup, before anything can fail.
void			PvzDebugInit(const char* theLogPath);

// Flushes and closes the log. Call at shutdown.
void			PvzDebugShutdown(void);

// Non-zero once PvzDebugInit() has succeeded. Cheap; used by the macros.
int				PvzDebugIsReady(void);

// --------------------------------------------------------------------------------------------------
// Runtime configuration
// --------------------------------------------------------------------------------------------------
void			PvzDebugSetLevel(int theLevel);
void			PvzDebugSetChannels(unsigned int theChannels);
void			PvzDebugEnableChannel(unsigned int theChannel, int theEnable);
void			PvzDebugSetAssertAction(int theAction);
int				PvzDebugGetLevel(void);
unsigned int	PvzDebugGetChannels(void);
int				PvzDebugGetAssertAction(void);

// Counters - handy for "did anything actually go wrong during that run?"
unsigned int	PvzDebugAssertCount(void);
unsigned int	PvzDebugErrorCount(void);
unsigned int	PvzDebugLogCount(void);

// --------------------------------------------------------------------------------------------------
// Logging
// --------------------------------------------------------------------------------------------------

// Gate check. theChannel==PVZ_CH_NONE always passes so generic messages are never lost.
int				PvzLogIsEnabled(int theLevel, unsigned int theChannel);

// Formatted log with source location. Prefer the PVZ_LOG* macros.
void			PvzLogEx(int theLevel, unsigned int theChannel, const char* theFile,
					int theLine, const char* theFormat, ...);

// Pre-formatted line, no location prefix. Used internally for banners and dumps.
void			PvzLogRaw(const char* theText);

// --------------------------------------------------------------------------------------------------
// Assertions
// --------------------------------------------------------------------------------------------------

// Records a failed invariant. Returns the action the caller should take.
// theFormat may be NULL / the variadic args may be omitted entirely.
int				PvzAssertReport(const char* theCondition, const char* theFile,
					int theLine, const char* theFormat, ...);

// Unconditional "this code path must not run". Returns the action to take.
int				PvzFailReport(const char* theFile, int theLine, const char* theFormat, ...);

// --------------------------------------------------------------------------------------------------
// Crash handling
// --------------------------------------------------------------------------------------------------

// Installs a top-level exception filter. After this, an access violation produces a
// log entry with the faulting address, the recent-message ring buffer, any registered
// state dump and a minidump, instead of silently killing the process.
void			PvzDebugInstallCrashHandler(void);

// Optional: a callback invoked during a crash report to describe game state.
// Registered by LawnApp/Board so the crash log can say which level/flag/wave we were on.
// Keep the callback simple - it runs inside an exception filter.
typedef void	(*PvzStateDumpFn)(void* theContext);
void			PvzDebugSetStateDump(PvzStateDumpFn theFn, void* theContext);

// Force pending output to disk.
void			PvzDebugFlush(void);

// Write the in-memory ring buffer into the log. Called automatically on crash;
// also useful from a debugger or a cheat key.
void			PvzDebugDumpRecent(void);

#if defined(__cplusplus)
} // extern "C"
#endif

// ==================================================================================================
// Macros
// ==================================================================================================

#if PVZ_DEBUG_ENABLED

	// -- logging -----------------------------------------------------------------------------------

	// PVZ_LOG(CHANNEL, LEVEL, fmt, ...)
	#define PVZ_LOG(theChannel, theLevel, ...)											\
		do {																			\
			if (PvzLogIsEnabled((theLevel), (unsigned int)(theChannel)))				\
				PvzLogEx((theLevel), (unsigned int)(theChannel), __FILE__, __LINE__,	\
					__VA_ARGS__);														\
		} while (false)

	#define PVZ_LOG_ERROR(theChannel, ...)	PVZ_LOG(theChannel, PVZ_LOG_ERROR, __VA_ARGS__)
	#define PVZ_LOG_WARN(theChannel, ...)	PVZ_LOG(theChannel, PVZ_LOG_WARN, __VA_ARGS__)
	#define PVZ_LOG_INFO(theChannel, ...)	PVZ_LOG(theChannel, PVZ_LOG_INFO, __VA_ARGS__)
	#define PVZ_LOG_VERBOSE(theChannel, ...)	PVZ_LOG(theChannel, PVZ_LOG_VERBOSE, __VA_ARGS__)

	// Fire exactly once per call site. For conditions that would otherwise repeat every frame.
	#define PVZ_LOG_ONCE(theChannel, theLevel, ...)										\
		do {																			\
			static int pvzAlreadyLogged_ = 0;											\
			if (!pvzAlreadyLogged_)														\
			{																			\
				pvzAlreadyLogged_ = 1;													\
				PVZ_LOG(theChannel, theLevel, __VA_ARGS__);								\
			}																			\
		} while (false)

	// Log the first hit, then at most one every theEveryNth hits.
	// PVZ_LOG_THROTTLED(PVZ_CH_ZOMBIE, PVZ_LOG_WARN, 100, "still bad")
	#define PVZ_LOG_THROTTLED(theChannel, theLevel, theEveryNth, ...)					\
		do {																			\
			static unsigned int pvzHitCount_ = 0;										\
			if ((pvzHitCount_ % (unsigned int)(theEveryNth)) == 0U)						\
				PVZ_LOG(theChannel, theLevel, __VA_ARGS__);								\
			pvzHitCount_++;																\
		} while (false)

	// -- assertions --------------------------------------------------------------------------------

	// PVZ_ASSERT(cond) or PVZ_ASSERT(cond, "fmt", args...)
	// Breaks into the debugger only when the configured action says so.
	//
	// The "" before __VA_ARGS__ is deliberate: it lets the message be omitted entirely
	// (the empty literal concatenates with the caller's format string when there is
	// one) without needing a defaulted parameter, which extern "C" makes awkward.
	#define PVZ_ASSERT(theCondition, ...)												\
		do {																			\
			if (!(theCondition))														\
			{																			\
				if (PvzAssertReport(#theCondition, __FILE__, __LINE__,					\
						"" __VA_ARGS__) == PVZ_ASSERT_BREAK)							\
				{																		\
					if (IsDebuggerPresent())											\
						__debugbreak();													\
				}																		\
			}																			\
		} while (false)

	// Same as PVZ_ASSERT but the condition is still evaluated when the module is
	// compiled out. Use when the condition has a side effect you rely on.
	#define PVZ_VERIFY(theCondition, ...)	PVZ_ASSERT(theCondition, ##__VA_ARGS__)

	// Index against a count. Casting through unsigned catches negative indices too,
	// which is the usual bug when a row/column comes back as -1.
	#define PVZ_ASSERT_INDEX(theIndex, theCount, theWhat)								\
		PVZ_ASSERT((unsigned int)(theIndex) < (unsigned int)(theCount),				\
			"%s index out of range: %d, count %d", (theWhat), (int)(theIndex), (int)(theCount))

	// Inclusive range check.
	#define PVZ_ASSERT_RANGE(theValue, theMin, theMax, theWhat)							\
		PVZ_ASSERT((theValue) >= (theMin) && (theValue) <= (theMax),					\
			"%s out of range: %d not in [%d, %d]", (theWhat), (int)(theValue),			\
			(int)(theMin), (int)(theMax))

	// Null check with a label.
	#define PVZ_ASSERT_PTR(thePointer, theWhat)											\
		PVZ_ASSERT((thePointer) != nullptr, "%s is NULL", (theWhat))

	// Unreachable / not-yet-implemented marker.
	#define PVZ_FAIL(...)																\
		do {																			\
			if (PvzFailReport(__FILE__, __LINE__, "" __VA_ARGS__) == PVZ_ASSERT_BREAK)	\
			{																			\
				if (IsDebuggerPresent())												\
					__debugbreak();														\
			}																			\
		} while (false)

	// -- tracing -----------------------------------------------------------------------------------

	// Two levels of indirection are needed: the inner one pastes, the outer one lets
	// __COUNTER__ expand to a number first.
	#define PVZ_CAT2(theA, theB)	theA##theB
	#define PVZ_CAT(theA, theB)		PVZ_CAT2(theA, theB)

	// Scope timer: logs "enter" on construction and "exit (N ms)" on destruction.
	// PVZ_TRACE_SCOPE(PVZ_CH_BOARD, "InitLevel")
	// The name is uniquified via __COUNTER__ so several scopes in one function are fine.
	#define PVZ_TRACE_SCOPE(theChannel, ...)											\
		PvzScopeTrace PVZ_CAT(pvzScopeTrace_, __COUNTER__)(theChannel, __FILE__,		\
			__LINE__, "" __VA_ARGS__)

	// Function entry/exit at VERBOSE. Use sparingly - it is per-call.
	// __FUNCTION__ is passed through "%s" rather than concatenated, because it is a
	// string literal on MSVC but a const array on other compilers.
	#define PVZ_TRACE_FN(theChannel)	PVZ_TRACE_SCOPE(theChannel, "%s", __FUNCTION__)

#else // PVZ_DEBUG_ENABLED == 0

	#define PVZ_LOG(theChannel, theLevel, ...)					((void)0)
	#define PVZ_LOG_ERROR(theChannel, ...)						((void)0)
	#define PVZ_LOG_WARN(theChannel, ...)						((void)0)
	#define PVZ_LOG_INFO(theChannel, ...)						((void)0)
	#define PVZ_LOG_VERBOSE(theChannel, ...)					((void)0)
	#define PVZ_LOG_ONCE(theChannel, theLevel, ...)				((void)0)
	#define PVZ_LOG_THROTTLED(theChannel, theLevel, n, ...)		((void)0)

	#define PVZ_ASSERT(theCondition, ...)						((void)0)
	#define PVZ_VERIFY(theCondition, ...)						((void)(theCondition))
	#define PVZ_ASSERT_INDEX(theIndex, theCount, theWhat)		((void)0)
	#define PVZ_ASSERT_RANGE(theValue, theMin, theMax, theWhat)	((void)0)
	#define PVZ_ASSERT_PTR(thePointer, theWhat)					((void)0)
	#define PVZ_FAIL(...)										((void)0)
	#define PVZ_TRACE_SCOPE(theChannel, ...)					((void)0)
	#define PVZ_TRACE_FN(theChannel)							((void)0)

#endif // PVZ_DEBUG_ENABLED

// --------------------------------------------------------------------------------------------------
// Scope trace helper. Only compiled when the module is enabled; the macro that
// constructs it compiles to nothing otherwise, so the type is not needed then.
// --------------------------------------------------------------------------------------------------
#if PVZ_DEBUG_ENABLED
#if defined(__cplusplus)

class PvzScopeTrace
{
public:
	unsigned int		mChannel;
	const char*			mFile;
	int					mLine;
	DWORD				mStartMs;
	char				mLabel[160];

public:
	PvzScopeTrace(unsigned int theChannel, const char* theFile, int theLine,
		const char* theFormat, ...);
	~PvzScopeTrace();

private:
	PvzScopeTrace(const PvzScopeTrace&);
	PvzScopeTrace& operator=(const PvzScopeTrace&);
};

#endif // __cplusplus
#endif // PVZ_DEBUG_ENABLED

#endif // __PVZDEBUG_H__
