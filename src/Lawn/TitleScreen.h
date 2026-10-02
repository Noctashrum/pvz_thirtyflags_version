#ifndef __TITLESCREEN_H__
#define __TITLESCREEN_H__

#include "../SexyAppFramework/Widget.h"
#include "../SexyAppFramework/ButtonListener.h"

class LawnApp;
namespace Sexy
{
	class HyperlinkWidget;
}

enum TitleState
{
	TITLESTATE_WAITING_FOR_FIRST_DRAW,
	TITLESTATE_POPCAP_LOGO,
	TITLESTATE_PARTNER_LOGO,
	TITLESTATE_SCREEN
};

class TitleScreen :public Sexy::Widget, public Sexy::ButtonListener
{
public:
	enum
	{
		TitleScreen_Start,
		TitleScreen_Register
	};

public:
	Sexy::HyperlinkWidget*	mStartButton;				//+0x8C
	float					mCurBarWidth;				//+0x90
	float					mTotalBarWidth; 			//+0x94
	float					mBarVel;					//+0x98
	float					mBarStartProgress;			//+0x9C
	bool					mRegisterClicked;			//+0xA0
	bool					mLoadingThreadComplete;		//+0xA1
	int						mTitleAge;					//+0xA4
	Sexy::KeyCode			mQuickLoadKey;				//+0xA8
	bool					mNeedRegister;				//+0xAC
	bool					mNeedShowRegisterBox;		//+0xAD
	bool					mDrawnYet;					//+0xAE
	bool					mNeedToInit;				//+0xAF
	float					mPrevLoadingPercent;		//+0xB0
	TitleState				mTitleState;				//+0xB4
	int						mTitleStateCounter;			//+0xB8
	int						mTitleStateDuration;		//+0xBC
	bool					mDisplayPartnerLogo;		//+0xC0
	bool					mLoaderScreenIsLoaded;		//+0xC1
	LawnApp*				mApp;						//+0xC4

public:
	TitleScreen(LawnApp* theApp);
	virtual ~TitleScreen();

	virtual void			Update();
	virtual void			Draw(Sexy::Graphics* g);
	virtual void			Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void			AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			ButtonPress(int theId);
	virtual void			ButtonDepress(int theId);
	virtual void			MouseDown(int x, int y, int theClickCount);
	virtual void			KeyDown(Sexy::KeyCode theKey);
	virtual void			KeyChar(SexyChar theChar);
	void					SetRegistered();
	void					DrawToPreload(Sexy::Graphics* g);
};

#endif
