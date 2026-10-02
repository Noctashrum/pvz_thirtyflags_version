#ifndef __CHALLENGESCREEN_H__
#define __CHALLENGESCREEN_H__

#include "LawnCommon.h"
#include "../SexyAppFramework/Dialog.h"

class LawnApp;
class ToolTipWidget;
class NewLawnButton;

enum ChallengePage
{
	CHALLENGE_PAGE_SURVIVAL,
	CHALLENGE_PAGE_CHALLENGE,
	CHALLENGE_PAGE_LIMBO,
	CHALLENGE_PAGE_PUZZLE,
	MAX_CHALLANGE_PAGES
};

enum UnlockingState
{
	UNLOCK_OFF,
	UNLOCK_SHAKING,
	UNLOCK_FADING
};

class ChallengeScreen : public Sexy::Widget, public Sexy::ButtonListener
{
private:
	enum
	{
		ChallengeScreen_Back = 100,
		ChallengeScreen_Mode = 200,
		ChallengeScreen_Page = 300
	};

public:
	NewLawnButton*				mBackButton;								//+0x8C
	Sexy::ButtonWidget*			mPageButton[MAX_CHALLANGE_PAGES];			//+0x90
	Sexy::ButtonWidget*			mChallengeButtons[NUM_CHALLENGE_MODES];		//+0xA0
	LawnApp*					mApp;										//+0x1C0
	ToolTipWidget*				mToolTip;									//+0x1C4
	ChallengePage				mPageIndex;									//+0x1C8
	bool						mCheatEnableChallenges;						//+0x1CC
	UnlockingState				mUnlockState;								//+0x1D0
	int							mUnlockStateCounter;						//+0x1D4
	int							mUnlockChallengeIndex;						//+0x1D8
	float						mLockShakeX;								//+0x1DC
	float						mLockShakeY;								//+0x1E0

public:
	ChallengeScreen(LawnApp* theApp, ChallengePage thePage);
	virtual ~ChallengeScreen();

	void						SetUnlockChallengeIndex(ChallengePage thePage, bool theIsIZombie = false);
	int							MoreTrophiesNeeded(int theChallengeIndex);
	bool						ShowPageButtons();
	void						UpdateButtons();
	int							AccomplishmentsNeeded(int theChallengeIndex);
	void						DrawButton(Sexy::Graphics* g, int theChallengeIndex);
	virtual void				Draw(Sexy::Graphics* g);
	virtual void				Update();
	virtual void				AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void				RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void				ButtonPress(int theId);
	virtual void				ButtonDepress(int theId);
	void						UpdateToolTip();
	virtual void				KeyChar(SexyChar theChar);

	bool						IsScaryPotterLevel(GameMode theGameMode);
	bool						IsIZombieLevel(GameMode theGameMode);
};

class ChallengeDefinition
{
public:
	GameMode					mChallengeMode;
	int							mChallengeIconIndex;
	ChallengePage				mPage;
	int							mRow;
	int							mCol;
	const SexyChar*				mChallengeName;
};
// 【三十旗改版】数组长度 +1，末位新增 GAMEMODE_THIRTY_FLAGS 条目
extern ChallengeDefinition gChallengeDefs[NUM_CHALLENGE_MODES + 1];  // 0x6A2BA0

ChallengeDefinition& GetChallengeDefinition(int theChallengeMode);

#endif