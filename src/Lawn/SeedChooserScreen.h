#ifndef __SEEDCHOOSERSCREEN_H__
#define __SEEDCHOOSERSCREEN_H__

#include "SeedPacket.h"
#include "../SexyAppFramework/Widget.h"

class Board;
class LawnApp;
class GameButton;
class ToolTipWidget;
namespace Sexy
{
	class MTRand;
}

enum ChosenSeedState
{
	SEED_FLYING_TO_BANK,
	SEED_IN_BANK,
	SEED_FLYING_TO_CHOOSER,
	SEED_IN_CHOOSER,
	SEED_PACKET_HIDDEN
};

class ChosenSeed
{
public:
	int						mX;									//+0x0
	int						mY;									//+0x4
	int						mTimeStartMotion;					//+0x8
	int						mTimeEndMotion;						//+0xC
	int						mStartX;							//+0x10
	int						mStartY;							//+0x14
	int						mEndX;								//+0x18
	int						mEndY;								//+0x1C
	SeedType				mSeedType;							//+0x20
	ChosenSeedState			mSeedState;							//+0x24
	int						mSeedIndexInBank;					//+0x28
	bool					mRefreshing;						//+0x2C
	int						mRefreshCounter;					//+0x30
	SeedType				mImitaterType;						//+0x34
	bool					mCrazyDavePicked;					//+0x38
};

enum SeedChooserState
{
	CHOOSE_NORMAL,
	CHOOSE_VIEW_LAWN
};

enum NotRecommend
{
	NOT_RECOMMENDED_NOCTURNAL,
	NOT_RECOMMENDED_NEEDS_POOL,
	NOT_RECOMMENDED_NEEDS_GRAVES,
	NOT_RECOMMENDED_NEEDS_FOG,
	NOT_RECOMMENDED_NEEDS_ROOF,
	NOT_RECOMMENDED_ON_ROOF,
	NOT_RECOMMENDED_FOR_CHALLENGE,
	NOT_RECOMMENDED_AT_NIGHT
};

class SeedChooserScreen : public Sexy::Widget
{
private:
	enum
	{
		SeedChooserScreen_Start = 100,
		SeedChooserScreen_Random = 101,
		SeedChooserScreen_ViewLawn = 102,
		SeedChooserScreen_Almanac = 103,
		SeedChooserScreen_Menu = 104,
		SeedChooserScreen_Store = 105,
		SeedChooserScreen_Imitater = 106
	};

public:
	GameButton*				mStartButton;						//+0x88
	GameButton*				mRandomButton;						//+0x8C
	GameButton*				mViewLawnButton;					//+0x90
	GameButton*				mStoreButton;						//+0x94
	GameButton*				mAlmanacButton;						//+0x98
	GameButton*				mMenuButton;						//+0x9C
	GameButton*				mImitaterButton;					//+0xA0
	ChosenSeed				mChosenSeeds[NUM_SEED_TYPES];		//+0xA4
	LawnApp*				mApp;								//+0xD10
	Board*					mBoard;								//+0xD14
	int						mNumSeedsToChoose;					//+0xD18
	int						mSeedChooserAge;					//+0xD1C
	int						mSeedsInFlight;						//+0xD20
	int						mSeedsInBank;						//+0xD24
	ToolTipWidget*			mToolTip;							//+0xD28
	int						mToolTipSeed;						//+0xD2C
	int						mLastMouseX;						//+0xD30
	int						mLastMouseY;						//+0xD34
	SeedChooserState		mChooseState;						//+0xD38
	int						mViewLawnTime;						//+0xD3C

public:
	SeedChooserScreen();
	~SeedChooserScreen();

	static int				PickFromWeightedArrayUsingSpecialRandSeed(TodWeightedArray* theArray, int theCount, Sexy::MTRand& theLevelRNG);
	void					CrazyDavePickSeeds();
	bool					Has7Rows();
	void					GetSeedPositionInChooser(int theIndex, int& x, int& y);
	void					GetSeedPositionInBank(int theIndex, int& x, int& y);
	unsigned int			SeedNotRecommendedToPick(SeedType theSeedType);
	bool					SeedNotAllowedToPick(SeedType theSeedType);
	bool					SeedNotAllowedDuringTrial(SeedType theSeedType);
	virtual void			Draw(Sexy::Graphics* g);
	void					UpdateViewLawn();
	void					LandFlyingSeed(ChosenSeed& theChosenSeed);
	void					UpdateCursor();
	virtual void			Update();
	bool					DisplayRepickWarningDialog(const SexyChar* theMessage);
	bool					FlyersAreComming();
	bool					FlyProtectionCurrentlyPlanted();
	bool					CheckSeedUpgrade(SeedType theSeedTypeTo, SeedType theSeedTypeFrom);
	void					OnStartButton();
	void					PickRandomSeeds();
	virtual void			ButtonDepress(int theId);
	SeedType				SeedHitTest(int x, int y);
	SeedType				FindSeedInBank(int theIndexInBank);
	void					EnableStartButton(bool theEnabled);
	void					ClickedSeedInBank(ChosenSeed& theChosenSeed);
	void					ClickedSeedInChooser(ChosenSeed& theChosenSeed);
	void					ShowToolTip();
	void					RemoveToolTip();
	void					CancelLawnView();
	virtual void			MouseUp(int x, int y, int theClickCount);
	void					UpdateImitaterButton();
	virtual void			MouseDown(int x, int y, int theClickCount);
	bool					PickedPlantType(SeedType theSeedType);
	void					CloseSeedChooser();
	virtual void			KeyDown(Sexy::KeyCode theKey);
	virtual void			KeyChar(SexyChar theChar);
	void					UpdateAfterPurchase();
};

#endif
