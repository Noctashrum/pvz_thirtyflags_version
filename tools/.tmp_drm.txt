#include "PopDRMComm.h"

//0x5D5AC0
PopDRMComm::PopDRMComm()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	mThread = NULL;
	mThreadID = NULL;
	mEvent = NULL;
	mHwnd = NULL;
	mDRMHwnd = NULL;
	mFileHandle = NULL;
	mDataPtr = NULL;
	mRequiredThread = GetCurrentThreadId();

	mLockMsg = RegisterClipboardFormat("PopCapDRM_EnableLocking");
	mTimingMsg = RegisterClipboardFormat("PopCapDRM_EnableTiming");
	mQueryMsg = RegisterClipboardFormat("PopCapDRM_QueryData");
	mConnectMsg = RegisterClipboardFormat("PopCapDRM_IPC");
	mResponseMsg = RegisterClipboardFormat("PopCapDRM_IPC_Response");
	mForceLockMsg = RegisterClipboardFormat("PopCapDRM_ForceLock");
	mBuyGameMsg = RegisterClipboardFormat("PopCapDRM_BuyGame");
	mShowScreenMsg = RegisterClipboardFormat("PopCapDRM_ShowScreen");
	mWaitMsg = RegisterClipboardFormat("PopCapDRM_WaitUI");
	mConnected = false;

	InitializeCriticalSection(&mStringCritSection);
	Start();
#endif
}

//0x5D5B60
PopDRMComm::~PopDRMComm()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	if (mThread)
	{
		PostThreadMessage(mThreadID, WM_QUIT, NULL, NULL);
		WaitForSingleObject(mThread, -1);
		CloseHandle(mThread);
	}

	if (mDataPtr)
	{
		UnmapViewOfFile(mDataPtr);
	}

	if (mFileHandle)
	{
		CloseHandle(mFileHandle);
	}

	DeleteCriticalSection(&mStringCritSection);
#endif
}

//0x5D5BC0
void PopDRMComm::Start()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	mEvent = CreateEvent(NULL, NULL, NULL, NULL);
	mThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PopDRMComm::ShowWindow, this, 0, (LPDWORD)&mThreadID);
	WaitForSingleObject(mEvent, -1);

	CloseHandle(mEvent);
	mEvent = NULL;
#endif
}

//0x5D5C10
void PopDRMComm::CreateData()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	if (mFileHandle == NULL)
	{
		char aName[16];
		sprintf(aName, "HWND%08.8X", mDRMHwnd);
		mFileHandle = ::CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x10000, aName);

		if (mFileHandle)
		{
			mDataPtr = ::MapViewOfFile(mFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		}
	}
#endif
}

//0x5D5C80
void PopDRMComm::EnableLocking()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	if (IsWindow(mDRMHwnd))
	{
		SendMessage(mDRMHwnd, mLockMsg, NULL, NULL);
	}
	else
	{
		mConnected = false;
	}
#endif
}

//0x5D5CB0
bool PopDRMComm::QueryData()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	if (IsWindow(mDRMHwnd))
	{
		return SendMessage(mDRMHwnd, mQueryMsg, 4, NULL) != NULL;
	}
	else
	{
		mConnected = false;
		return false;
	}
#else
	return true;
#endif
}

//0x5D5CE0
void PopDRMComm::BuyGame()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	if (!IsWindow(mDRMHwnd))
	{
		mConnected = false;
		return;
	}

	CreateData();
	EnterCriticalSection(&mStringCritSection);
	memcpy(mDataPtr, "_ingame", strlen("_ingame"));
	
	SendMessage(mDRMHwnd, mBuyGameMsg, 0, 1);
	LeaveCriticalSection(&mStringCritSection);
#endif
}

//0x5D5D60
bool PopDRMComm::Connect()
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	if (!mConnected)
	{
		mEvent = CreateEvent(NULL, NULL, NULL, NULL);

		PostMessage(HWND_BROADCAST, mConnectMsg, GetCurrentProcessId(), (LPARAM)mHwnd);
		WaitForSingleObject(mEvent, 100);

		CloseHandle(mEvent);
		mEvent = NULL;
	}
	return mConnected;
#else
	return false;
#endif
}

//0x5D5DC0
void WINAPI PopDRMComm::ShowWindow(LPVOID lpThreadParameter)
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = PopDRMComm::WindowProc;
	wc.lpszClassName = _S("PopDRMComm_Game");
	wc.hInstance = GetModuleHandle(NULL);
	RegisterClass(&wc);

	HWND aHWnd = CreateWindow("PopDRMComm_Game", NULL, WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

	PopDRMComm* aDrm = (PopDRMComm*)lpThreadParameter;
	aDrm->mHwnd = aHWnd;

	if (IsWindow(aHWnd))
	{
		SetWindowLong(aDrm->mHwnd, GWL_USERDATA, (LONG)aDrm);
		SetEvent(aDrm->mEvent);
	}

	MSG msg;
	BOOL bRet;
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	if (IsWindow(aDrm->mHwnd))
	{
		DestroyWindow(aDrm->mHwnd);
	}
#endif
}

//0x5D5EC0
LRESULT CALLBACK PopDRMComm::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _ENABLE_DRM_FOR_DEBUGGING
	PopDRMComm* aDrm = (PopDRMComm*)GetWindowLong(hwnd, GWL_USERDATA);

	if (aDrm)
	{
		if ((uMsg == aDrm->mResponseMsg) && (GetCurrentProcessId() == (DWORD)wParam))
		{
			aDrm->mDRMHwnd = (HWND)lParam;
			aDrm->mConnected = true;

			if (aDrm->mEvent)
			{
				SetEvent(aDrm->mEvent);
			}
		}
	}
#endif

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

