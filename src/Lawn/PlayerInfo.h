#ifndef __PLAYERINFO_H__
#define __PLAYERINFO_H__

#include "LawnCommon.h"

#define MAX_POTTED_PLANTS		200
#define PURCHASE_COUNT_OFFSET	1000
#define MAX_PURCHASES			80

enum SeedType;
enum GardenType;
enum DrawVariation;

enum PottedPlantAge
{
	PLANTAGE_SPROUT,
	PLANTAGE_SMALL,
	PLANTAGE_MEDIUM,
	PLANTAGE_FULL
};

enum PottedPlantNeed
{
	PLANTNEED_NONE,
	PLANTNEED_WATER,
	PLANTNEED_FERTILIZER,
	PLANTNEED_BUGSPRAY,
	PLANTNEED_PHONOGRAPH
};

class PottedPlant
{
public:
	enum FacingDirection
	{
		FACING_RIGHT,
		FACING_LEFT
	};

public:
	SeedType			mSeedType;							//+0x0
	GardenType			mWhichZenGarden;					//+0x4
	int					mX;									//+0x8
	int					mY;									//+0xC
	FacingDirection		mFacing;							//+0x10

	__time64_t			mLastWateredTime;					//+0x18
	DrawVariation		mDrawVariation;						//+0x20
	PottedPlantAge		mPlantAge;							//+0x24
	int					mTimesFed;							//+0x28
	int					mFeedingsPerGrow;					//+0x2C
	PottedPlantNeed		mPlantNeed;							//+0x30

	__time64_t			mLastNeedFulfilledTime;				//+0x38
	__time64_t			mLastFertilizedTime;				//+0x40
	__time64_t			mLastChocolateTime;					//+0x48
	__time64_t			mFutureAttribute[1];				//+0x50

public:
	void				InitializePottedPlant(SeedType theSeedType);
};

class DataSync;
class PlayerInfo
{
public:
	SexyString			mName;								//+0x0
	ulong				mUseSeq;							//+0x1C
	ulong				mId;								//+0x20
	int					mLevel;								//+0x24
	int					mCoins;								//+0x28
	int					mFinishedAdventure;					//+0x2C
	int					mChallengeRecords[100];				//+0x30
	int					mPurchases[MAX_PURCHASES];			//+0x1C0
	int					mPlayTimeActivePlayer;				//+0x300
	int					mPlayTimeInactivePlayer;			//+0x304
	int					mHasUsedCheatKeys;					//+0x308
	int					mHasWokenStinky;					//+0x30C
	int					mDidntPurchasePacketUpgrade;		//+0x310
	long				mLastStinkyChocolateTime;			//+0x314
	int					mStinkyPosX;						//+0x318
	int					mStinkyPosY;						//+0x31C
	int					mHasUnlockedMinigames;				//+0x320
	int					mHasUnlockedPuzzleMode;				//+0x324
	int					mHasNewMiniGame;					//+0x328
	int					mHasNewScaryPotter;					//+0x32C
	int					mHasNewIZombie;						//+0x330
	int					mHasNewSurvival;					//+0x334
	int					mHasUnlockedSurvivalMode;			//+0x338
	int					mNeedsMessageOnGameSelector;		//+0x33C
	int					mNeedsMagicTacoReward;				//+0x340
	int					mHasSeenStinky;						//+0x344
	int					mHasSeenUpsell;						//+0x348
	int					mPlaceHolderPlayerStats;
	int					mNumPottedPlants;					//+0x350
	PottedPlant			mPottedPlant[MAX_POTTED_PLANTS];	//+0x358

public:
	PlayerInfo();

	void				Reset();
	void				AddCoins(int theAmount);
	void				SyncSummary(DataSync& theSync);
	void				SyncDetails(DataSync& theSync);
	void				DeleteUserFiles();
	void				LoadDetails();
	void				SaveDetails();
	int					GetLevel() const;
	void				SetLevel(int theLevel);
	void				ResetChallengeRecord(GameMode theGameMode);
};

#endif
