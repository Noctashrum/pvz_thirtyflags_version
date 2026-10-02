#ifndef __POPDRMCOMM_H__
#define __POPDRMCOMM_H__

#include "../SexyAppFramework/CritSect.h"

class PopDRMComm
{
	class Web
	{
		enum EDownloaderStatus
		{
			STATUS_Done,
			STATUS_NotStarted,
			STATUS_Pending,
			STATUS_FileNotFound,
			STATUS_ServerError,
			STATUS_Aborted,
			STATUS_SocketError,
			STATUS_InvalidAddress,
			STATUS_ConnectFailed,
			STATUS_Disconnected,
			STATUS_InternalError,
		};
	private:
		PopDRMComm*					mComm;
		uint						mBeginGetMsg;
		uint						mGetTransferStatusMsg;
		uint						mFinishGetMsg;
	};

private:
	enum
	{
		PopCapDRM_EnableLocking,
		PopCapDRM_EnableTiming,
		PopCapDRM_QueryData,
		PopCapDRM_IPC,
		PopCapDRM_IPC_Response,
		PopCapDRM_ForceLock,
		PopCapDRM_BuyGame,
		PopCapDRM_ShowScreen,
		PopCapDRM_WaitUI,
		PopCapDRM_NumMessages
	};

private:
	HWND							mHwnd;					//+0x0
	HWND							mDRMHwnd;				//+0x4
	HANDLE							mFileHandle;			//+0x8
	void*							mDataPtr;				//+0xC
	bool							mConnected;				//+0x10
	HANDLE							mEvent;					//+0x14
	HANDLE							mThread;				//+0x18
	uint							mThreadID;				//+0x1C
	uint							mRequiredThread;		//+0x20
	int								mLockMsg;				//+0x24
	int								mTimingMsg;				//+0x28
	int								mQueryMsg;				//+0x2C
	int								mConnectMsg;			//+0x30
	int								mResponseMsg;			//+0x34
	int								mForceLockMsg;			//+0x38
	int								mBuyGameMsg;			//+0x3C
	int								mShowScreenMsg;			//+0x40
	int								mWaitMsg;				//+0x44
	CRITICAL_SECTION				mStringCritSection;		//+0x48
	
public:
	PopDRMComm();
	~PopDRMComm();

	void							Start();
	void							EnableLocking();
	void							BuyGame();
	bool							Connect();
	bool							QueryData();
	static void	WINAPI				ShowWindow(LPVOID lpThreadParameter);
	void							CreateData();
	static LRESULT CALLBACK			WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
