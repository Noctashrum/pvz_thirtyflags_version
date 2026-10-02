#ifndef __COIN_H__
#define __COIN_H__

#include "GameObject.h"
#include "PlayerInfo.h"

class HitResult;
enum AttachmentID;

enum CoinType
{
	COIN_NONE,
	COIN_SILVER,
	COIN_GOLD,
	COIN_DIAMOND,
	COIN_SUN,
	COIN_SMALLSUN,
	COIN_LARGESUN,
	COIN_FINAL_SEED_PACKET,
	COIN_TROPHY,
	COIN_SHOVEL,
	COIN_ALMANAC,
	COIN_CARKEYS,
	COIN_VASE,
	COIN_WATERING_CAN,
	COIN_TACO,
	COIN_NOTE,
	COIN_USABLE_SEED_PACKET,
	COIN_PRESENT_PLANT,
	COIN_AWARD_MONEY_BAG,
	COIN_AWARD_PRESENT,
	COIN_AWARD_BAG_DIAMOND,
	COIN_AWARD_SILVER_SUNFLOWER,
	COIN_AWARD_GOLD_SUNFLOWER,
	COIN_CHOCOLATE,
	COIN_AWARD_CHOCOLATE,
	COIN_PRESENT_MINIGAMES,
	COIN_PRESENT_PUZZLE_MODE,
	COIN_PRESENT_SURVIVAL_MODE,
};

enum CoinMotion
{
	COIN_MOTION_FROM_SKY,
	COIN_MOTION_FROM_SKY_SLOW,
	COIN_MOTION_FROM_PLANT,
	COIN_MOTION_COIN,
	COIN_MOTION_LAWNMOWER_COIN,
	COIN_MOTION_FROM_PRESENT,
	COIN_MOTION_FROM_BOSS
};

enum CoinID
{
	COINID_NULL
};

class Coin : public GameObject
{
public:
	float					mPosX;					//+0x24
	float					mPosY;					//+0x28
	float					mVelX;					//+0x2C
	float					mVelY;					//+0x30
	float					mScale;					//+0x34
	bool					mDead;					//+0x38
	int						mFadeCount;				//+0x3C
	float					mCollectX;				//+0x40
	float					mCollectY;				//+0x44
	int						mGroundY;				//+0x48
	int						mCoinAge;				//+0x4C
	bool					mIsBeingCollected;		//+0x50
	int						mDisappearCounter;		//+0x54
	CoinType				mType;					//+0x58
	CoinMotion				mCoinMotion;			//+0x5C
	AttachmentID			mAttachmentID;			//+0x60
	float					mCollectionDistance;	//+0x64
	SeedType				mUsableSeedType;		//+0x68
	PottedPlant				mPottedPlantSpec;		//+0x70
	bool					mNeedsBouncyArrow;		//+0xC8
	bool					mHasBouncyArrow;		//+0xC9
	bool					mHitGround;				//+0xCA
	int						mTimesDropped;			//+0xCC

public:
	Coin();
	~Coin();

	void					CoinInitialize(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion);
	void					MouseDown(int x, int y, int theClickCount);
	bool					MouseHitTest(int theX, int theY, HitResult* theHitResult);
	void					Die();
	void					StartFade();
	void					Update();
	void					Draw(Sexy::Graphics* g);
	void					Collect();
	int						GetSunValue();
	static int				GetCoinValue(CoinType theCoinType);
	void					UpdateFade();
	void					UpdateFall();
	void					ScoreCoin();
	void					UpdateCollected();
	Sexy::Color				GetColor();
	bool					IsMoney();
	bool					IsSun();
	float					GetSunScale();
	inline bool				IsOnGround() { return false; }
	SeedType				GetFinalSeedPacketType();
	bool					IsLevelAward();
	bool					CoinGetsBouncyArrow();
	void					FanOutCoins(CoinType theCoinType, int theNumCoins);
	int						GetDisappearTime();
	void					DroppedUsableSeed();
	void					PlayCollectSound();
	void					TryAutoCollectAfterLevelAward();
	bool					IsPresentWithAdvice();
	void					PlayLaunchSound();
	void					PlayGroundSound();

	static bool				IsMoney(CoinType theType);
};

#endif
