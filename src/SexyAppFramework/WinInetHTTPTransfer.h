#ifndef __WININETHTTPTRANSFER_H__
#define __WININETHTTPTRANSFER_H__

#include "Common.h"

namespace Sexy
{
	class WinInetHTTPTransfer
	{
	public:
		enum EResult
		{
			RESULT_DONE,
			RESULT_NOT_STARTED,
			RESULT_NOT_COMPLETED,
			RESULT_NOT_FOUND,
			RESULT_HTTP_ERROR,
			RESULT_HTTP_REDIRECT,
			RESULT_ABORTED,
			RESULT_SOCKET_ERROR,
			RESULT_INVALID_ADDR,
			RESULT_CONNECT_FAIL,
			RESULT_DISCONNECTED,
			RESULT_INTERNAL_ERROR
		};
		
	private:
		std::string						mSpecifiedBaseURL;				//+0x4
		std::string						mSpecifiedRelURL;				//+0x20
		std::string						mURL;							//+0x3C
		std::string						mProto;							//+0x58
		std::string						mUserName;						//+0x74
		std::string						mUserPass;						//+0x90
		std::string						mHost;							//+0xAC
		int								mPort;							//+0xC8
		std::string						mPath;							//+0xCC
		std::string						mAction;						//+0xE8
		std::string						mUserAgent;						//+0x104
		std::string						mPostContentType;				//+0x120
		std::string						mPostData;						//+0x13C
		FILE*							mFP;							//+0x158
		bool							mUsingFile;						//+0x15C
		std::string						mContent;						//+0x160
		int								mContentLength;					//+0x17C
		int								mCurContentLength;				//+0x180
		bool							mTransferPending;				//+0x184
		bool							mThreadRunning;					//+0x185
		bool							mExiting;						//+0x186
		bool							mAborted;						//+0x187
		EResult							mResult;						//+0x188
		RTL_CRITICAL_SECTION			mFileCritSection;				//+0x18C
		
	protected:
		static std::string				GetAbsURL(const std::string& theBaseURL, const std::string& theRelURL);
		void							GetHelper(const std::string& theURL);
		void							PostHelper(const std::string& theURL, const std::string& theParams, const char* theContentType);
		void							PrepareTransfer(const std::string& theURL);
		void							StartTransfer();
		void							TransferThreadProc();
		void							Fail(EResult theResult);
		
	public:
		WinInetHTTPTransfer();
		virtual ~WinInetHTTPTransfer();
		
		bool							SetOutputFile(const std::wstring& theFileName);
		bool							SetOutputFile(const std::string& theFileName);
		void							Get(const std::string& theURL);
		void							Get(const std::string& theBaseURL, const std::string& theRelURL);
		void							Post(const std::string& theURL, const std::string& theParams);
		void							Post(const std::string& theBaseURL, const std::string& theRelURL, const std::string& theParams);
		void							PostMultiPart(const std::string& theURL, const std::string& theParams, const std::string& theContentType);
		void							PostMultiPart(const std::string& theBaseURL, const std::string& theRelURL, const std::string& theParams, const std::string& theContentType);
		void							Reset();
		void							Abort();
		void							WaitFor();
		EResult							GetResultCode();
		std::string						GetContent();
		static void						TransferThreadProcStub(void* theArg);
	};
};

#endif
