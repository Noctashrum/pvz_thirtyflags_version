#ifndef __GRIDITEM_H__
#define __GRIDITEM_H__

#define NUM_MOTION_TRAIL_FRAMES 12

class LawnApp;
class Board;
class Zombie;
namespace Sexy
{
	class Graphics;
};
enum ReanimationID;
enum ParticleSystemID;
enum SeedType;
enum ZombieType;

class MotionTrailFrame
{
public:
	float					mPosX;
	float					mPosY;
	float					mAnimTime;
};

enum GridItemType
{
	GRIDITEM_NONE,
	GRIDITEM_GRAVESTONE,
	GRIDITEM_CRATER,
	GRIDITEM_LADDER,
	GRIDITEM_PORTAL_CIRCLE,
	GRIDITEM_PORTAL_SQUARE,
	GRIDITEM_BRAIN,
	GRIDITEM_SCARY_POT,
	GRIDITEM_SQUIRREL,
	GRIDITEM_ZEN_TOOL,
	GRIDITEM_STINKY,
	GRIDITEM_RAKE,
	GRIDITEM_IZOMBIE_BRAIN
};

enum GridItemState
{
	GRIDITEM_STATE_NORMAL,
	GRIDITEM_STATE_GRAVESTONE_SPECIAL,
	GRIDITEM_STATE_PORTAL_CLOSED,
	GRIDITEM_STATE_SCARY_POT_QUESTION,
	GRIDITEM_STATE_SCARY_POT_LEAF,
	GRIDITEM_STATE_SCARY_POT_ZOMBIE,
	GRIDITEM_STATE_SQUIRREL_WAITING,
	GRIDITEM_STATE_SQUIRREL_PEEKING,
	GRIDITEM_STATE_SQUIRREL_RUNNING_UP,
	GRIDITEM_STATE_SQUIRREL_RUNNING_DOWN,
	GRIDITEM_STATE_SQUIRREL_RUNNING_LEFT,
	GRIDITEM_STATE_SQUIRREL_RUNNING_RIGHT,
	GRIDITEM_STATE_SQUIRREL_CAUGHT,
	GRIDITEM_STATE_SQUIRREL_ZOMBIE,
	GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN,
	GRIDITEM_STATE_ZEN_TOOL_FERTILIZER,
	GRIDITEM_STATE_ZEN_TOOL_BUG_SPRAY,
	GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH,
	GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN,
	GRIDITEM_STINKY_WALKING_LEFT,
	GRIDITEM_STINKY_TURNING_LEFT,
	GRIDITEM_STINKY_WALKING_RIGHT,
	GRIDITEM_STINKY_TURNING_RIGHT,
	GRIDITEM_STINKY_SLEEPING,
	GRIDITEM_STINKY_FALLING_ASLEEP,
	GRIDITEM_STINKY_WAKING_UP,
	GRIDITEM_STATE_RAKE_ATTRACTING,
	GRIDITEM_STATE_RAKE_WAITING,
	GRIDITEM_STATE_RAKE_TRIGGERED,
	GRIDITEM_STATE_BRAIN_SQUISHED
};

enum ScaryPotType
{
	SCARYPOT_NONE,
	SCARYPOT_SEED,
	SCARYPOT_ZOMBIE,
	SCARYPOT_SUN
};

class GridItem
{
public:
	LawnApp*				mApp;											//+0x0
	Board*					mBoard;											//+0x4
	GridItemType			mGridItemType;									//+0x8
	GridItemState			mGridItemState;									//+0xC
	int						mGridX;											//+0x10
	int						mGridY;											//+0x14
	int						mGridItemCounter;								//+0x18
	int						mRenderOrder;									//+0x1C
	bool					mDead;											//+0x20
	float					mPosX;											//+0x24
	float					mPosY;											//+0x28
	float					mGoalX;											//+0x2C
	float					mGoalY;											//+0x30
	ReanimationID			mGridItemReanimID;								//+0x34
	ParticleSystemID		mGridItemParticleID;							//+0x38
	ZombieType				mZombieType;									//+0x3C
	SeedType				mSeedType;										//+0x40
	ScaryPotType			mScaryPotType;									//+0x44
	bool					mHighlighted;									//+0x48
	int						mTransparentCounter;							//+0x4C
	int						mSunCount;										//+0x50
	MotionTrailFrame		mMotionTrailFrames[NUM_MOTION_TRAIL_FRAMES];	//+0x54
	int						mMotionTrailCount;								//+0xE4

public:
	GridItem();

	void					DrawLadder(Sexy::Graphics* g);
	void					DrawCrater(Sexy::Graphics* g);
	void					DrawGraveStone(Sexy::Graphics* g);
	void					GridItemDie();
	void					AddGraveStoneParticles();
	void					DrawGridItem(Sexy::Graphics* g);
	void					DrawGridItemOverlay(Sexy::Graphics* g);
	void					OpenPortal();
	void					Update();
	void					ClosePortal();
	void					DrawScaryPot(Sexy::Graphics* g);
	void					UpdateScaryPot();
	void					UpdatePortal();
	void					DrawSquirrel(Sexy::Graphics* g);
	void					UpdateRake();
	Zombie*					RakeFindZombie();
	void					DrawIZombieBrain(Sexy::Graphics* g);
	void					UpdateBrain();
	void					DrawStinky(Sexy::Graphics* g);
	bool					IsOpenPortal();
};

#endif
