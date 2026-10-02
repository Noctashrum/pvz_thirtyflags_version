#include "WinInetHTTPTransfer.h"
#include "AutoCrit.h"
#include <WinInet.h>

using namespace Sexy;

//0x5AD110
WinInetHTTPTransfer::WinInetHTTPTransfer()
{
	mThreadRunning = false;
	mFP = NULL;
	mUsingFile = false;
	mResult = EResult::RESULT_NOT_STARTED;

	InitializeCriticalSection(&mFileCritSection);
}

//0x5AD210¡¢0x5AD230
WinInetHTTPTransfer::~WinInetHTTPTransfer()
{
	Abort();
	while (mThreadRunning)
	{
		Sleep(20);
	}

	DeleteCriticalSection(&mFileCritSection);
}

std::string WinInetHTTPTransfer::GetAbsURL(const std::string& theBaseURL, const std::string& theRelURL)
{
	std::string aURL;

	if (theRelURL.substr(0, 7).compare("http://") == 0)
	{
		aURL = theRelURL;
	}
	else if (theRelURL.substr(0, 1).compare("/") == 0)
	{
		int aFirstSlashPos = theBaseURL.find('/', 7);
		if (aFirstSlashPos != -1)
		{
			aURL = theBaseURL.substr(0, aFirstSlashPos) + theRelURL;
		}
		else
		{
			aURL = theBaseURL + theRelURL;
		}
	}
	else
	{
		int aLastSlashPos = theBaseURL.rfind('/');
		if (aLastSlashPos >= 7)
		{
			aURL = theBaseURL.substr(0, aLastSlashPos + 1) + theRelURL;
		}
		else
		{
			aURL = theBaseURL + "/" + theRelURL;
		}
	}

	return aURL;
}

//0x5AD450
void WinInetHTTPTransfer::Get(const std::string& theURL)
{
	mSpecifiedBaseURL = "";
	mSpecifiedRelURL = theURL;

	GetHelper(theURL);
}

void WinInetHTTPTransfer::Post(const std::string& theURL, const std::string& theParams)
{
	mSpecifiedBaseURL = "";
	mSpecifiedRelURL = theURL;
	PostHelper(theURL, theParams, nullptr);
}

void WinInetHTTPTransfer::PostMultiPart(const std::string& theURL, const std::string& theParams, const std::string& theContentType)
{
	mSpecifiedBaseURL = "";
	mSpecifiedRelURL = theURL;
	PostHelper(theURL, theParams, theContentType.c_str());
}

void WinInetHTTPTransfer::Get(const std::string& theBaseURL, const std::string& theRelURL)
{
	mSpecifiedBaseURL = theBaseURL;
	mSpecifiedRelURL = theRelURL;

	GetHelper(GetAbsURL(theBaseURL, theRelURL));
}

void WinInetHTTPTransfer::Post(const std::string& theBaseURL, const std::string& theRelURL, const std::string& theParams)
{
	mSpecifiedBaseURL = theBaseURL;
	mSpecifiedRelURL = theRelURL;

	PostHelper(GetAbsURL(theBaseURL, theRelURL), theParams, nullptr);
}

void WinInetHTTPTransfer::PostMultiPart(const std::string& theBaseURL, const std::string& theRelURL, const std::string& theParams, const std::string& theContentType)
{
	mSpecifiedBaseURL = theBaseURL;
	mSpecifiedRelURL = theRelURL;

	PostHelper(GetAbsURL(theBaseURL, theRelURL), theParams, theContentType.c_str());
}

bool WinInetHTTPTransfer::SetOutputFile(const std::wstring& theFileName)
{
	if (mFP && mUsingFile)
	{
		fclose(mFP);
	}

	mFP = _wfopen(theFileName.c_str(), L"wb");
	mUsingFile = true;
}

bool WinInetHTTPTransfer::SetOutputFile(const std::string& theFileName)
{
	if (mFP && mUsingFile)
	{
		fclose(mFP);
	}

	mFP = fopen(theFileName.c_str(), "wb");
	mUsingFile = true;
}

//0x5AD480
void WinInetHTTPTransfer::Reset()
{
	if (mThreadRunning)
	{
		Abort();
		WaitFor();
	}

	mResult = EResult::RESULT_NOT_STARTED;

	mContent.erase();
	mExiting = false;
	mAborted = false;

	mURL.erase();
	mProto.erase();
	mUserName.erase();
	mUserPass.erase();
	mHost.erase();
	mPath.erase();
	mAction.erase();
	mUserAgent.erase();

	mContentLength = 0;
	mCurContentLength = 0;
}

//0x5AD540
void WinInetHTTPTransfer::Abort()
{
	mAborted = true;
	mExiting = true;
}

//0x5AD550
void WinInetHTTPTransfer::WaitFor()
{
	while (mTransferPending)
	{
		Sleep(1);
	}
}

//0x5AD570
std::string WinInetHTTPTransfer::GetContent()
{
	if (mResult == EResult::RESULT_NOT_COMPLETED)
		return "";

	std::string aContent;

	if (mUsingFile)
	{
		AutoCrit aAutoCrit(&mFileCritSection);

		long aFileLen = ftell(mFP);
		aContent.resize(aFileLen);

		fseek(mFP, 0, SEEK_SET);
		fread((void*)aContent.data(), 1, aFileLen, mFP);
	}
	else
	{
		aContent = mContent;
	}

	return aContent;
}

//0x5AD710
void WinInetHTTPTransfer::TransferThreadProcStub(void* theArg)
{
	WinInetHTTPTransfer* aTransfer = (WinInetHTTPTransfer*)theArg;
	
	aTransfer->TransferThreadProc();
	
	aTransfer->mTransferPending = false;
	aTransfer->mThreadRunning = false;
}

//0x5AD730
void WinInetHTTPTransfer::GetHelper(const std::string& theURL)
{
	PrepareTransfer(theURL);

	mAction = "GET";
	mUserAgent = "Mozilla/4.0 (compatible; Opera 4.0)";
	mPostContentType = "";
	mPostData = "";

	StartTransfer();
}

void WinInetHTTPTransfer::PostHelper(const std::string& theURL, const std::string& theParams, const char* theContentType)
{
	PrepareTransfer(theURL);

	mAction = "POST";
	if (theContentType)
	{
		mPostContentType = "Content-Type: multipart/form-data; boundary=" + std::string(theContentType) + "\r\n";
	}
	else
	{
		mPostContentType = "Content-Type: application/x-www-form-urlencoded\r\n";
	}
	mPostData = theParams;

	StartTransfer();
}

//0x5AD790
void WinInetHTTPTransfer::PrepareTransfer(const std::string& theURL)
{
	Reset();

	mURL = theURL;
	mExiting = false;
	mAborted = false;
	mResult = EResult::RESULT_NOT_COMPLETED;

	mContent.erase();

	size_t aSSPos = mURL.find("://");
	mProto = mURL.substr(0, aSSPos);

	if (mURL.compare(0, mURL.size(), "https", strlen("https")) == 0)
	{
		mPort = 443;
	}
	else
	{
		mPort = 80;
	}

	if (aSSPos != std::string::npos)
	{
		size_t aPos = aSSPos + strlen("://");
		size_t aSlashPos = mURL.find("/", aPos);

		if (aSlashPos != std::string::npos)
		{
			mPath = mURL.substr(aSlashPos);
		}
		else
		{
			mPath = "";
		}

		size_t aEndPos = (aSlashPos == std::string::npos) ? mURL.size() : aSlashPos;
		std::string aHost = mURL.substr(aPos, aEndPos - aPos);

		size_t aSemiPos = aHost.find(':');
		size_t aSemi2Pos = aHost.rfind(':');
		size_t aAtPos = aHost.find('@');

		if (aAtPos != std::string::npos)
		{
			if ((aSemiPos != std::string::npos) && (aSemiPos <= aAtPos))
			{
				mUserName = aHost.substr(0, aSemiPos);
				mUserPass = aHost.substr(aSemiPos + 1, aAtPos - aSemiPos - 1);
			}
			else
			{
				Fail(EResult::RESULT_INVALID_ADDR);
			}

			if ((aSemi2Pos != std::string::npos) && (aSemi2Pos != aSemiPos))
			{
				if (aSemi2Pos < aAtPos)
				{
					Fail(EResult::RESULT_INVALID_ADDR);
				}
				else
				{
					mPort = atoi(aHost.substr(aSemi2Pos + 1).c_str());
					mHost = aHost.substr(aAtPos + 1, aSemi2Pos - aAtPos - 1);
				}
			}
			else
			{
				mHost = aHost.substr(aAtPos + 1);
			}
		}
		else if (aSemiPos != aSemi2Pos)
		{
			Fail(EResult::RESULT_INVALID_ADDR);
		}
		else if (aSemiPos != std::string::npos)
		{
			mHost = aHost.substr(0, aSemiPos);
			mPort = atoi(aHost.substr(aSemiPos + 1).c_str());
		}
		else
		{
			mHost = aHost;
		}
	}
}

//0x5ADC70
void WinInetHTTPTransfer::StartTransfer()
{
	if (mResult != EResult::RESULT_NOT_COMPLETED)
		return;

	mTransferPending = true;
	mThreadRunning = true;

	DWORD aThreadId;
	HANDLE aHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WinInetHTTPTransfer::TransferThreadProcStub, (LPVOID)this, 0, &aThreadId);

	if (aHandle == NULL)
	{
		mTransferPending = false;
		mThreadRunning = false;
		mResult = EResult::RESULT_INTERNAL_ERROR;
	}
	else
	{
		CloseHandle(aHandle);
	}
}

//0x5ADCD0
void WinInetHTTPTransfer::TransferThreadProc()
{
	HINTERNET hInternet[3] = { NULL };
	int aCurIndex = 0;

	while (!mAborted)
	{
		if (mExiting)
			break;

		switch (aCurIndex)
		{
		case 0:
		{
			hInternet[aCurIndex] = InternetOpenA(mUserAgent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
			if (hInternet[aCurIndex] == NULL)
			{
				mAborted = true;
			}

			aCurIndex++;
			break;
		}

		case 1:
		{
			hInternet[aCurIndex] = InternetConnectA(
				hInternet[aCurIndex - 1],
				mHost.c_str(),
				mPort,
				mUserName.c_str(),
				mUserPass.c_str(),
				INTERNET_SERVICE_HTTP,
				INTERNET_FLAG_NO_CACHE_WRITE,
				0);

			if (hInternet[aCurIndex] == NULL)
			{
				mAborted = true;
			}

			aCurIndex++;
			break;
		}

		case 2:
		{
			DWORD aFlags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
			if (mProto.compare(0, mProto.size(), "https", strlen("https")) == 0)
			{
				aFlags |= INTERNET_FLAG_SECURE;
			}
			
			hInternet[aCurIndex] = HttpOpenRequestA(hInternet[aCurIndex - 1], mAction.c_str(), mPath.c_str(), NULL, NULL, NULL, aFlags, 0);
			if (hInternet[aCurIndex] == NULL)
			{
				mAborted = true;
			}
			else
			{
				BOOL aResult;
				if (mAction.compare(0, mAction.size(), "POST", strlen("POST")) == 0)
				{
					aResult = HttpSendRequestA(
						hInternet[aCurIndex], 
						mPostContentType.c_str(), 
						mPostContentType.size(), 
						(LPVOID)mPostData.data(), 
						mPostData.size());
				}
				else
				{
					aResult = HttpSendRequestA(hInternet[aCurIndex], NULL, NULL, NULL, NULL);
				}

				if (aResult == FALSE)
				{
					mExiting = true;
					mResult = EResult::RESULT_HTTP_ERROR;
				}
				else
				{
					long aStatusCode = 0;
					DWORD aBufferLen = sizeof(aStatusCode), aIndex = 0;
					if (!HttpQueryInfoA(hInternet[aCurIndex], HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &aStatusCode, &aBufferLen, &aIndex))
					{
						mAborted = true;
					}

					if (aStatusCode == HTTP_STATUS_NOT_FOUND)
					{
						Fail(EResult::RESULT_NOT_FOUND);
					}
					else if (aStatusCode != HTTP_STATUS_OK)
					{
						Fail(EResult::RESULT_HTTP_ERROR);
					}

					if (!mExiting)
					{
						long aContentLength;
						aBufferLen = sizeof(aContentLength);
						HttpQueryInfoA(hInternet[aCurIndex], HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH, &aContentLength, &aBufferLen, &aIndex);

						mContentLength = 0;
						mCurContentLength = 0;
						aCurIndex++;
					}
				}
			}
		}

		default:
		{
			char aBuf[1024];
			DWORD aBufLen = LENGTH(aBuf);
			if (!InternetReadFile(hInternet[2], aBuf, aBufLen, &aBufLen))
			{
				Fail(EResult::RESULT_DISCONNECTED);
			}
			else if (aBufLen == 0)
			{
				mExiting = true;
			}
			else
			{
				if (mFP && mUsingFile)
				{
					AutoCrit aAutoCrit(&mFileCritSection);

					if (fwrite(aBuf, 1, aBufLen, mFP) != aBufLen)
					{
						Fail(EResult::RESULT_INTERNAL_ERROR);
					}
					else
					{
						fflush(mFP);
					}
				}
				else
				{
					mContent = aBuf;
				}
			}
		}
		}
	}

	for (int i = 0; i < LENGTH(hInternet); ++i)
	{
		if (hInternet[i])
		{
			InternetCloseHandle(hInternet[i]);
		}
	}

	if (mAborted)
	{
		Fail(EResult::RESULT_ABORTED);
	}
	else if (mResult == EResult::RESULT_NOT_COMPLETED)
	{
		mResult = EResult::RESULT_DONE;
	}

	{
		AutoCrit aAutoCrit(&mFileCritSection);

		if (mFP)
		{
			fclose(mFP);
		}
		mFP = NULL;
		mUsingFile = false;
	}
}

//0x5AE170
void WinInetHTTPTransfer::Fail(EResult theResult)
{
	mResult = theResult;
	mExiting = true;
}

WinInetHTTPTransfer::EResult WinInetHTTPTransfer::GetResultCode()
{
	return mResult;
}
