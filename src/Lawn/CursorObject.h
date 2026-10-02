#ifndef __CURSOROBJECT_H__
#define __CURSOROBJECT_H__

#include "GameObject.h"

enum CoinID;
enum PlantID;
enum ReanimationID;
enum SeedType;

enum CursorType
{
	CURSOR_TYPE_NORMAL,
	CURSOR_TYPE_PLANT_FROM_BANK,
	CURSOR_TYPE_PLANT_FROM_USABLE_COIN,
	CURSOR_TYPE_PLANT_FROM_GLOVE,
	CURSOR_TYPE_PLANT_FROM_DUPLICATOR,
	CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW,
	CURSOR_TYPE_SHOVEL,
	CURSOR_TYPE_HAMMER,
	CURSOR_TYPE_COBCANNON_TARGET,
	CURSOR_TYPE_WATERING_CAN,
	CURSOR_TYPE_FERTILIZER,
	CURSOR_TYPE_BUG_SPRAY,
	CURSOR_TYPE_PHONOGRAPH,
	CURSOR_TYPE_CHOCOLATE,
	CURSOR_TYPE_GLOVE,
	CURSOR_TYPE_MONEY_SIGN,
	CURSOR_TYPE_WHEEELBARROW,
	CURSOR_TYPE_TREE_FOOD
};

class CursorObject : public GameObject
{
public:
	int						mSeedBankIndex;			//+0x24
	SeedType				mType;					//+0x28
	SeedType				mImitaterType;			//+0x2C
	CursorType				mCursorType;			//+0x30
	CoinID					mCoinID;				//+0x34
	PlantID					mGlovePlantID;			//+0x38
	PlantID					mDuplicatorPlantID;		//+0x3C
	PlantID					mCobCannonPlantID;		//+0x40
	int						mHammerDownCounter;		//+0x44
	ReanimationID			mReanimCursorID;		//+0x48

public:
	CursorObject();

	void					Update();
	void					Draw(Sexy::Graphics* g);
	void					Die();
};

class CursorPreview : public GameObject
{
public:
	int						mGridX;
	int						mGridY;

public:
	CursorPreview();

	void					Update();
	void					Draw(Sexy::Graphics* g);
};

#endif
