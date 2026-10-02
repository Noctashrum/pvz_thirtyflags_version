#ifndef __LAWNMOWER_H__
#define __LAWNMOWER_H__

#include "LawnCommon.h"
#include "../SexyAppFramework/Rect.h"

class Board;
class Zombie;
class LawnApp;
namespace Sexy
{
	class Graphics;
};
enum ReanimationID;

enum LawnMowerType
{
	LAWNMOWER_LAWN,
	LAWNMOWER_POOL,
	LAWNMOWER_ROOF,
	LAWNMOWER_SUPER_MOWER,
	NUM_MOWER_TYPES
};

enum LawnMowerState
{
	MOWER_ROLLING_IN,
	MOWER_READY,
	MOWER_TRIGGERED,
	MOWER_SQUISHED
};

enum MowerHeight
{
	MOWER_HEIGHT_LAND,
	MOWER_HEIGHT_DOWN_TO_POOL,
	MOWER_HEIGHT_IN_POOL,
	MOWER_HEIGHT_UP_TO_LAND
};

class LawnMower
{
public:
	LawnApp*					mApp;						//+0x0
	Board*						mBoard;						//+0x4
	float						mPosX;						//+0x8
	float						mPosY;						//+0xC
	int							mRenderOrder;				//+0x10
	int							mRow;						//+0x14
	int							mAnimTicksPerFrame;			//+0x18
	ReanimationID				mReanimID;					//+0x1C
	int							mChompCounter;				//+0x20
	int							mRollingInCounter;			//+0x24
	int							mSquishedCounter;			//+0x28
	LawnMowerState				mMowerState;				//+0x2C
	bool						mDead;						//+0x30
	bool						mVisible;					//+0x31
	LawnMowerType				mMowerType;					//+0x34
	float						mAltitude;					//+0x38
	MowerHeight					mMowerHeight;				//+0x3C
	int							mLastPortalX;				//+0x40

public:
	void						LawnMowerInitialize(int theRow);
	void						StartMower();
	void						Update();
	void						Draw(Sexy::Graphics* g);
	void						Die();
	Sexy::Rect					GetLawnMowerAttackRect();
	void						UpdatePool();
	void						MowZombie(Zombie* theZombie);
	void						SquishMower();
	void						EnableSuperMower(bool theEnable);
};

#endif
