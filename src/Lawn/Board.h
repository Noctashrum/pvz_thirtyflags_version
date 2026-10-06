#ifndef __BOARD_H__
#define __BOARD_H__

#include "LawnCommon.h"
#include "../TodLib/DataArray.h"
#include "../SexyAppFramework/ButtonListener.h"

#include "Plant.h"
#include "Zombie.h"
#include "Projectile.h"
#include "Coin.h"
#include "LawnMower.h"
#include "GridItem.h"

class CursorObject;
class CursorPreview;
class GameButton;
class MessageWidget;
class SeedBank;
class ToolTipWidget;
class CutScene;
class Challenge;
class Reanimation;
class DataSync;
class TodParticleSystem;
namespace Sexy
{
	class ButtonWidget;
	class MTRand;
}
enum MessageStyle;
enum BoardResult;
enum CursorType;
enum ParticleEffect;

constexpr int	MAX_ZOMBIES_IN_WAVE			= 50;
// 【三十旗改版】单波僵尸数量上限（仅作安全护栏，正常由点数预算自然收敛）。
// 注意：开场预加载已改为「按僵尸类型」而非「按个体」，因此放宽此上限不会拖慢加载。
constexpr int	TF_MAX_ZOMBIES_PER_WAVE		= 50;
// 【三十旗改版】30 面旗 x 每旗 6 波 = 180 波。
// 引擎假设「关卡开始时一次性确定完整波次表」（CutScene::PlaceStreetZombies 会遍历全部波次），
// 因此这里必须真正容纳 180 波，而不是每旗重建小块表。
constexpr int	MAX_ZOMBIE_WAVES			= 180;
constexpr int	MAX_GRAVE_STONES			= MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y;
constexpr int	MAX_POOL_GRID_SIZE			= 10;
constexpr int	MAX_RENDER_ITEMS			= 2048;
constexpr int	PROGRESS_METER_COUNTER		= 150;

constexpr int	WIDE_BOARD_WIDTH			= 800;
// 【性能】每行僵尸分桶容量（超出则不予分桶，仅影响极端堆叠时的碰撞查找范围）
constexpr int	TF_ROW_BUCKET_MAX			= 256;
constexpr int	BOARD_OFFSET				= 220;
constexpr int	BOARD_EDGE					= -100;
constexpr int	BOARD_IMAGE_WIDTH_OFFSET	= 1180;
constexpr int   BOARD_ICE_START				= 800;
constexpr int   LAWN_XMIN					= 40;
constexpr int   LAWN_YMIN					= 80;
// 【三十旗改版】日间布局（5 行 x 100px）的草坪底边。未开放的行走此线以下不绘制，
// 形成原版「左侧草坪缺口」那种观感；第 6 行 y=580 已越界，故三十旗按 5 行设计。
constexpr int   TF_LAWN_YMAX_DAY			= LAWN_YMIN + 5 * 100;
constexpr int   HIGH_GROUND_HEIGHT			= 30;

/* #################################################################################################### */

struct LawnFPoint
{
	float							x;
	float							y;
};

/* #################################################################################################### */

enum GameObjectType
{
	OBJECT_TYPE_NONE,
	OBJECT_TYPE_PLANT,
	OBJECT_TYPE_PROJECTILE,
	OBJECT_TYPE_COIN,
	OBJECT_TYPE_SEEDPACKET,
	OBJECT_TYPE_SHOVEL,
	OBJECT_TYPE_WATERING_CAN,
	OBJECT_TYPE_FERTILIZER,
	OBJECT_TYPE_BUG_SPRAY,
	OBJECT_TYPE_PHONOGRAPH,
	OBJECT_TYPE_CHOCOLATE,
	OBJECT_TYPE_GLOVE,
	OBJECT_TYPE_MONEY_SIGN,
	OBJECT_TYPE_WHEELBARROW,
	OBJECT_TYPE_TREE_FOOD,
	OBJECT_TYPE_NEXT_GARDEN,
	OBJECT_TYPE_MENU_BUTTON,
	OBJECT_TYPE_STORE_BUTTON,
	OBJECT_TYPE_SLOT_MACHINE_HANDLE,
	OBJECT_TYPE_SCARY_POT,
	OBJECT_TYPE_STINKY,
	OBJECT_TYPE_TREE_OF_WISDOM
};

class HitResult
{
public:
	void*							mObject;
	GameObjectType					mObjectType;
};

/* #################################################################################################### */

enum RenderObjectType
{
	RENDER_ITEM_COIN,
	RENDER_ITEM_PROJECTILE,
	RENDER_ITEM_ZOMBIE,
	RENDER_ITEM_ZOMBIE_SHADOW,
	RENDER_ITEM_ZOMBIE_BUNGEE_TARGET,
	RENDER_ITEM_PLANT,
	RENDER_ITEM_PLANT_OVERLAY,
	RENDER_ITEM_PLANT_MAGNET_ITEMS,
	RENDER_ITEM_CURSOR_PREVIEW,
	RENDER_ITEM_PARTICLE,
	RENDER_ITEM_REANIMATION,
	RENDER_ITEM_ICE,
	RENDER_ITEM_TOP_UI,
	RENDER_ITEM_FOG,
	RENDER_ITEM_STORM,
	RENDER_ITEM_BOTTOM_UI,
	RENDER_ITEM_BACKDROP,
	RENDER_ITEM_DOOR_MASK,
	RENDER_ITEM_COIN_BANK,
	RENDER_ITEM_PROJECTILE_SHADOW,
	RENDER_ITEM_MOWER,
	RENDER_ITEM_SCREEN_FADE,
	RENDER_ITEM_BOSS_PART,
	RENDER_ITEM_GRID_ITEM,
	RENDER_ITEM_GRID_ITEM_OVERLAY
};

class RenderItem
{
public:
	RenderObjectType				mRenderObjectType;
	int								mZPos;
	union
	{
		GameObject*					mGameObject;
		Plant*						mPlant;
		Zombie*						mZombie;
		Coin*						mCoin;
		Projectile*					mProjectile;
		CursorPreview*				mCursorPreview;
		TodParticleSystem*			mParticleSytem;
		Reanimation*				mReanimation;
		GridItem*					mGridItem;
		LawnMower*					mMower;
		BossPart					mBossPart;
		int							mBoardGridY;
	};
};
bool RenderItemSortFunc(const RenderItem& theItem1, const RenderItem& theItem2);

/* #################################################################################################### */

struct ZombiePicker
{
	int								mZombieCount;
	int								mZombiePoints;
	int								mZombieTypeCount[NUM_ZOMBIE_TYPES];
	int								mAllWavesZombieTypeCount[NUM_ZOMBIE_TYPES];
};

void								ZombiePickerInitForWave(ZombiePicker* theZombiePicker);
void								ZombiePickerInit(ZombiePicker* theZombiePicker);

/* #################################################################################################### */

enum PlantingReason
{
	PLANTING_OK,
	PLANTING_NOT_HERE,
	PLANTING_ONLY_ON_GRAVES,
	PLANTING_ONLY_IN_POOL,
	PLANTING_ONLY_ON_GROUND,
	PLANTING_NEEDS_POT,
	PLANTING_NOT_ON_ART,
	PLANTING_NOT_PASSED_LINE,
	PLANTING_NEEDS_UPGRADE,
	PLANTING_NOT_ON_GRAVE,
	PLANTING_NOT_ON_CRATER,
	PLANTING_NOT_ON_WATER,
	PLANTING_NEEDS_GROUND,
	PLANTING_NEEDS_SLEEPING
};

enum PlantPriority
{
	TOPPLANT_EATING_ORDER,
	TOPPLANT_DIGGING_ORDER,
	TOPPLANT_BUNGEE_ORDER,
	TOPPLANT_CATAPULT_ORDER,
	TOPPLANT_ZEN_TOOL_ORDER,
	TOPPLANT_ANY,
	TOPPLANT_ONLY_NORMAL_POSITION,
	TOPPLANT_ONLY_FLYING,
	TOPPLANT_ONLY_PUMPKIN,
	TOPPLANT_ONLY_UNDER_PLANT
};

struct PlantsOnLawn
{
	Plant*							mUnderPlant;
	Plant*							mPumpkinPlant;
	Plant*							mFlyingPlant;
	Plant*							mNormalPlant;
};

/* #################################################################################################### */

struct BungeeDropGrid
{
	TodWeightedGridArray			mGridArray[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
	int								mGridArrayCount;
};

/* #################################################################################################### */

enum InGameButtons
{
	GAME_MENU_BUTTON,
	GAME_STORE_BUTTON
};

enum AdviceType
{
	ADVICE_NONE = -1,
	ADVICE_CLICK_ON_SUN,
	ADVICE_CLICKED_ON_SUN,
	ADVICE_CLICKED_ON_COIN,
	ADVICE_SEED_REFRESH,
	ADVICE_CANT_AFFORD_PLANT,
	ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES,
	ADVICE_PLANT_LILYPAD_ON_WATER,
	ADVICE_PLANT_TANGLEKELP_ON_WATER,
	ADVICE_PLANT_SEASHROOM_ON_WATER,
	ADVICE_PLANT_POTATOE_MINE_ON_LILY,
	ADVICE_PLANT_WRONG_ART_TYPE,
	ADVICE_PLANT_NEED_POT,
	ADVICE_PLANT_NOT_ON_GRAVE,
	ADVICE_PLANT_NOT_ON_CRATER,
	ADVICE_CANT_PLANT_THERE,
	ADVICE_PLANT_NOT_ON_WATER,
	ADVICE_PLANTING_NEEDS_GROUND,
	ADVICE_BEGHOULED_DRAG_TO_MATCH_3,
	ADVICE_BEGHOULED_MATCH_3,
	ADVICE_BEGHOULED_MATCH_4,
	ADVICE_BEGHOULED_SAVE_SUN,
	ADVICE_BEGHOULED_USE_CRATER_1,
	ADVICE_BEGHOULED_USE_CRATER_2,
	ADVICE_PLANT_NOT_PASSED_LINE,
	ADVICE_PLANT_ONLY_ON_REPEATERS,
	ADVICE_PLANT_ONLY_ON_MELONPULT,
	ADVICE_PLANT_ONLY_ON_SUNFLOWER,
	ADVICE_PLANT_ONLY_ON_SPIKEWEED,
	ADVICE_PLANT_ONLY_ON_KERNELPULT,
	ADVICE_PLANT_ONLY_ON_MAGNETSHROOM,
	ADVICE_PLANT_ONLY_ON_FUMESHROOM,
	ADVICE_PLANT_ONLY_ON_LILYPAD,
	ADVICE_PLANT_NEEDS_REPEATER,
	ADVICE_PLANT_NEEDS_MELONPULT,
	ADVICE_PLANT_NEEDS_SUNFLOWER,
	ADVICE_PLANT_NEEDS_SPIKEWEED,
	ADVICE_PLANT_NEEDS_KERNELPULT,
	ADVICE_PLANT_NEEDS_MAGNETSHROOM,
	ADVICE_PLANT_NEEDS_FUMESHROOM,
	ADVICE_PLANT_NEEDS_LILYPAD,
	ADVICE_SLOT_MACHINE_PULL,
	ADVICE_HUGE_WAVE,
	ADVICE_SHOVEL_REFRESH,
	ADVICE_PORTAL_RELOCATING,
	ADVICE_SLOT_MACHINE_COLLECT_SUN,
	ADVICE_DESTORY_POTS_TO_FINISIH_LEVEL,
	ADVICE_USE_SHOVEL_ON_POTS,
	ADVICE_ALMOST_THERE,
	ADVICE_ZOMBIQUARIUM_CLICK_TROPHY,
	ADVICE_ZOMBIQUARIUM_COLLECT_SUN,
	ADVICE_ZOMBIQUARIUM_CLICK_TO_FEED,
	ADVICE_ZOMBIQUARIUM_BUY_SNORKEL,
	ADVICE_I_ZOMBIE_PLANTS_NOT_REAL,
	ADVICE_I_ZOMBIE_NOT_PASSED_LINE,
	ADVICE_I_ZOMBIE_LEFT_OF_LINE,
	ADVICE_SLOT_MACHINE_SPIN_AGAIN,
	ADVICE_I_ZOMBIE_EAT_ALL_BRAINS,
	ADVICE_PEASHOOTER_DIED,
	ADVICE_STINKY_SLEEPING,
	ADVICE_BEGHOULED_NO_MOVES,
	ADVICE_PLANT_SUNFLOWER5,
	ADVICE_PLANTING_NEED_SLEEPING,
	ADVICE_CLICK_TO_CONTINUE,
	ADVICE_SURVIVE_FLAGS,
	ADVICE_UNLOCKED_MODE,
	ADVICE_NEED_WHEELBARROW,
	NUM_ADVICE_TYPES
};

enum BackgroundType
{
	BACKGROUND_1_DAY,
	BACKGROUND_2_NIGHT,
	BACKGROUND_3_POOL,
	BACKGROUND_4_FOG,
	BACKGROUND_5_ROOF,
	BACKGROUND_6_BOSS,
	BACKGROUND_MUSHROOM_GARDEN,
	BACKGROUND_GREENHOUSE,
	BACKGROUND_ZOMBIQUARIUM,
	BACKGROUND_TREEOFWISDOM
};

enum DebugTextMode
{
	DEBUG_TEXT_NONE,
	DEBUG_TEXT_ZOMBIE_SPAWN,
	DEBUG_TEXT_MUSIC,
	DEBUG_TEXT_MEMORY,
	DEBUG_TEXT_COLLISION
};

enum GridSquareType
{
	GRIDSQUARE_NONE,
	GRIDSQUARE_GRASS,
	GRIDSQUARE_DIRT,
	GRIDSQUARE_POOL,
	GRIDSQUARE_HIGH_GROUND
};

enum PlantRowType
{
	PLANTROW_DIRT,
	PLANTROW_NORMAL,
	PLANTROW_POOL,
	PLANTROW_HIGH_GROUND
};

enum TutorialState
{
	TUTORIAL_OFF,
	TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER,
	TUTORIAL_LEVEL_1_PLANT_PEASHOOTER,
	TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER,
	TUTORIAL_LEVEL_1_COMPLETED,
	TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER,
	TUTORIAL_LEVEL_2_PLANT_SUNFLOWER,
	TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER,
	TUTORIAL_LEVEL_2_COMPLETED,
	TUTORIAL_MORESUN_PICK_UP_SUNFLOWER,
	TUTORIAL_MORESUN_PLANT_SUNFLOWER,
	TUTORIAL_MORESUN_REFRESH_SUNFLOWER,
	TUTORIAL_MORESUN_COMPLETED,
	TUTORIAL_SLOT_MACHINE_PULL,
	TUTORIAL_SLOT_MACHINE_COMPLETED,
	TUTORIAL_SHOVEL_PICKUP,
	TUTORIAL_SHOVEL_DIG,
	TUTORIAL_SHOVEL_KEEP_DIGGING,
	TUTORIAL_SHOVEL_COMPLETED,
	TUTORIAL_ZOMBIQUARIUM_BUY_SNORKEL,
	TUTORIAL_ZOMBIQUARIUM_BOUGHT_SNORKEL,
	TUTORIAL_ZOMBIQUARIUM_CLICK_TROPHY,
	TUTORIAL_ZEN_GARDEN_PICKUP_WATER,
	TUTORIAL_ZEN_GARDEN_WATER_PLANT,
	TUTORIAL_ZEN_GARDEN_KEEP_WATERING,
	TUTORIAL_ZEN_GARDEN_VISIT_STORE,
	TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS,
	TUTORIAL_ZEN_GARDEN_COMPLETED,
	TUTORIAL_WHACK_A_ZOMBIE_BEFORE_PICK_SEED,
	TUTORIAL_WHACK_A_ZOMBIE_PICK_SEED,
	TUTORIAL_WHACK_A_ZOMBIE_COMPLETED
};

class Board : public Sexy::Widget, public Sexy::ButtonListener
{
public:
	LawnApp*						mApp;													//+0x8C
	DataArray<Zombie>				mZombies;												//+0x90
	DataArray<Plant>				mPlants;												//+0xAC
	DataArray<Projectile>			mProjectiles;											//+0xC8
	DataArray<Coin>					mCoins;													//+0xE4
	DataArray<LawnMower>			mLawnMowers;											//+0x100
	DataArray<GridItem>				mGridItems;												//+0x11C
	CursorObject*					mCursorObject;											//+0x138
	CursorPreview*					mCursorPreview;											//+0x13C
	MessageWidget*					mAdvice;												//+0x140
	SeedBank*						mSeedBank;												//+0x144
	GameButton*						mMenuButton;											//+0x148
	GameButton*						mStoreButton;											//+0x14C
	bool							mIgnoreMouseUp;											//+0x150
	ToolTipWidget*					mToolTip;												//+0x154
	Sexy::Font*						mDebugFont;												//+0x158
	CutScene*						mCutScene;												//+0x15C
	Challenge*						mChallenge;												//+0x160
	bool							mPaused;												//+0x164
	GridSquareType					mGridSquareType[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];		//+0x168
	int								mGridCelLook[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];			//+0x240
	int								mGridCelOffset[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y][2];	//+0x318
	int								mGridCelFog[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y + 1];		//+0x4C8
	bool							mEnableGraveStones;										//+0x5C4
	int								mSpecialGraveStoneX;									//+0x5C8
	int								mSpecialGraveStoneY;									//+0x5CC
	float							mFogOffset;												//+0x5D0
	int								mFogBlownCountDown;										//+0x5D4
	PlantRowType					mPlantRow[MAX_GRID_SIZE_Y];								//+0x5D8
	int								mWaveRowGotLawnMowered[MAX_GRID_SIZE_Y];				//+0x5F0
	int								mBonusLawnMowersRemaining;								//+0x608
	int								mIceMinX[MAX_GRID_SIZE_Y];								//+0x60C
	int								mIceTimer[MAX_GRID_SIZE_Y];								//+0x624
	ParticleSystemID				mIceParticleID[MAX_GRID_SIZE_Y];						//+0x63C
	TodSmoothArray					mRowPickingArray[MAX_GRID_SIZE_Y];						//+0x654
	ZombieType						mZombiesInWave[MAX_ZOMBIE_WAVES][MAX_ZOMBIES_IN_WAVE];	//+0x6B4
	bool							mZombieAllowed[100];									//+0x54D4
	int								mSunCountDown;											//+0x5538
	int								mNumSunsFallen;											//+0x553C
	int								mShakeCounter;											//+0x5540
	int								mShakeAmountX;											//+0x5544
	int								mShakeAmountY;											//+0x5548
	BackgroundType					mBackground;											//+0x554C
	int								mLevel;													//+0x5550
	int								mSodPosition;											//+0x5554
	int								mPrevMouseX;											//+0x5558
	int								mPrevMouseY;											//+0x555C
	int								mSunMoney;												//+0x5560
	int								mNumWaves;												//+0x5564
	int								mMainCounter;											//+0x5568
	int								mEffectCounter;											//+0x556C
	int								mDrawCount;												//+0x5570
	int								mRiseFromGraveCounter;									//+0x5574
	int								mOutOfMoneyCounter;										//+0x5578
	int								mCurrentWave;											//+0x557C
	int								mTotalSpawnedWaves;										//+0x5580
	TutorialState					mTutorialState;											//+0x5584
	ParticleSystemID				mTutorialParticleID;									//+0x5588
	int								mTutorialTimer;											//+0x558C
	int								mLastBungeeWave;										//+0x5590
	int								mZombieHealthToNextWave;								//+0x5594
	int								mZombieHealthWaveStart;									//+0x5598
	int								mZombieCountDown;										//+0x559C
	int								mZombieCountDownStart;									//+0x55A0
	int								mHugeWaveCountDown;										//+0x55A4
	bool							mHelpDisplayed[NUM_ADVICE_TYPES];						//+0x55A8
	AdviceType						mHelpIndex;												//+0x55EC
	bool							mFinalBossKilled;										//+0x55F0
	bool							mShowShovel;											//+0x55F1
	int								mCoinBankFadeCount;										//+0x55F4
	DebugTextMode					mDebugTextMode;											//+0x55F8
	bool							mLevelComplete;											//+0x55FC
	int								mBoardFadeOutCounter;									//+0x5600
	int								mNextSurvivalStageCounter;								//+0x5604
	int								mScoreNextMowerCounter;									//+0x5608
	bool							mLevelAwardSpawned;										//+0x560C
	int								mProgressMeterWidth;									//+0x5610
	int								mFlagRaiseCounter;										//+0x5614
	int								mIceTrapCounter;										//+0x5618
	int								mBoardRandSeed;											//+0x561C
	ParticleSystemID				mPoolSparklyParticleID;									//+0x5620
	ReanimationID					mFwooshID[MAX_GRID_SIZE_Y][12];							//+0x5624
	int								mFwooshCountDown;										//+0x5744
	int								mTimeStopCounter;										//+0x5748
	bool							mDroppedFirstCoin;										//+0x574C
	int								mFinalWaveSoundCounter;									//+0x5750
	int								mCobCannonCursorDelayCounter;							//+0x5754
	int								mCobCannonMouseX;										//+0x5758
	int								mCobCannonMouseY;										//+0x575C
	bool							mKilledYeti;											//+0x5760
	bool							mMustacheMode;											//+0x5761
	bool							mSuperMowerMode;										//+0x5762
	bool							mFutureMode;											//+0x5763
	bool							mPinataMode;											//+0x5764
	bool							mDanceMode;												//+0x5765
	bool							mDaisyMode;												//+0x5766
	bool							mSukhbirMode;											//+0x5767
	BoardResult						mPrevBoardResult;										//+0x5768
	int								mTriggeredLawnMowers;									//+0x576C
	int								mPlayTimeActiveLevel;									//+0x5770
	int								mPlayTimeInactiveLevel;									//+0x5774
	int								mMaxSunPlants;											//+0x5778
	DWORD							mStartDrawTime;											//+0x577C
	DWORD							mIntervalDrawTime;										//+0x5780
	int								mIntervalDrawCountStart;								//+0x5784
	float							mMinFPS;												//+0x5788
	int								mPreloadTime;											//+0x578C
	int								mGameID;												//+0x5790
	int								mGravesCleared;											//+0x5794
	int								mPlantsEaten;											//+0x5798
	int								mPlantsShoveled;										//+0x579C
	int								mCoinsCollected;										//+0x57A0
	int								mDiamondsCollected;										//+0x57A4
	int								mPottedPlantsCollected;									//+0x57A8
	int								mChocolateCollected;									//+0x57AC

public:
	Board(LawnApp* theApp);
	virtual ~Board();

	void							DisposeBoard();
	int								CountSunBeingCollected();
	void							DrawGameObjects(Sexy::Graphics* g);
	void							ClearCursor();
	bool							AreEnemyZombiesOnScreen();
	LawnMower*						FindLawnMowerInRow(int theRow);
	inline bool						SyncState(DataSync& theDataSync) { /* 未发现 */return true; }
	void							SaveGame(const std::string& theFileName);
	bool							LoadGame(const std::string& theFileName);
	void							InitLevel();
	void							DisplayAdvice(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex);
	void							StartLevel();
	Plant*							AddPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	Projectile*						AddProjectile(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType);
	Coin*							AddCoin(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion);
	void							RefreshSeedPacketFromCursor();
	ZombieType						PickGraveRisingZombieType(int theZombiePoints);
	ZombieType						PickZombieType(int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker);
	int								PickRowForNewZombie(ZombieType theZombieType);
	Zombie*							AddZombie(ZombieType theZombieType, int theFromWave);
	void							SpawnZombieWave();
	void							RemoveAllZombies();
	void							RemoveCutsceneZombies();
	void							SpawnZombiesFromGraves();
	PlantingReason					CanPlantAt(int theGridX, int theGridY, SeedType theSeedType);
	virtual void					MouseMove(int x, int y);
	virtual void					MouseDrag(int x, int y);
	virtual void					MouseDown(int x, int y, int theClickCount);
	virtual void					MouseUp(int x, int y, int theClickCount);
	virtual void					KeyChar(SexyChar theChar);
	virtual void					KeyUp(Sexy::KeyCode theKey) { ; }
	virtual void					KeyDown(Sexy::KeyCode theKey);
	virtual void					Update();
	void							UpdateLayers();
	virtual void					Draw(Sexy::Graphics* g);
	void							DrawBackdrop(Sexy::Graphics* g);
	virtual void					ButtonMouseEnter(int theId) { ; }
	virtual void					ButtonMouseLeave(int theId) { ; }
	virtual void					ButtonPress(int theId) { ; }
	void							AddSunMoney(int theAmount);
	bool							TakeSunMoney(int theAmount);
	bool							CanTakeSunMoney(int theAmount);
	void							Pause(bool thePause);
	inline bool						MakeEasyZombieType() { /* 未发现 */return false; }
	void							TryToSaveGame();
	bool							NeedSaveGame();
	bool							RowCanHaveZombies(int theRow);
	void							ProcessDeleteQueue();
	bool							ChooseSeedsOnCurrentLevel();
	int								GetNumSeedsInBank();
	bool							StageIsNight();
	bool							StageHasPool();
	bool							StageHas6Rows();
	bool							StageHasFog();
	bool							StageHasGraveStones();
	int								PixelToGridX(int theX, int theY);
	int								PixelToGridY(int theX, int theY);
	int								GridToPixelX(int theGridX, int theGridY);
	int								GridToPixelY(int theGridX, int theGridY);
	int								PixelToGridXKeepOnBoard(int theX, int theY);
	int								PixelToGridYKeepOnBoard(int theX, int theY);
	void							UpdateGameObjects();
	bool							MouseHitTest(int x, int y, HitResult* theHitResult);
	void							MouseDownWithPlant(int x, int y, int theClickCount);
	void							MouseDownWithTool(int x, int y, int theClickCount, CursorType theCursorType);
	inline void						MouseDownNormal(int x, int y, int theClickCount) { /* 未发现 */; }
	bool							CanInteractWithBoardButtons();
	void							DrawProgressMeter(Sexy::Graphics* g);
	void							UpdateToolTip();
	Plant*							GetTopPlantAt(int theGridX, int theGridY, PlantPriority thePriority);
	void							GetPlantsOnLawn(int theGridX, int theGridY, PlantsOnLawn* thePlantOnLawn);
	int								CountSunFlowers();
	int								GetSeedPacketPositionX(int theIndex);
	void							AddGraveStones(int theGridX, int theCount, Sexy::MTRand& theLevelRNG);
	int								GetGraveStoneCount();
	void							ZombiesWon(Zombie* theZombie = nullptr);
	void							DrawLevel(Sexy::Graphics* g);
	void							DrawShovel(Sexy::Graphics* g);
	void							UpdateZombieSpawning();
	void							UpdateSunSpawning();
	void							ClearAdvice(AdviceType theHelpIndex);
	bool							RowCanHaveZombieType(int theRow, ZombieType theZombieType);
	int								NumberZombiesInWave(int theWaveIndex);
	int								TotalZombiesHealthInWave(int theWaveIndex);
	void							DrawDebugText(Sexy::Graphics* g);
	void							DrawUICoinBank(Sexy::Graphics* g);
	void							ShowCoinBank(int theDuration = 1000);
	void							FadeOutLevel();
	void							DrawFadeOut(Sexy::Graphics* g);
	void							DrawIce(Sexy::Graphics* g, int theGridY);
	bool							IsIceAt(int theGridX, int theGridY);
	ZombieID						ZombieGetID(Zombie* theZombie);
	Zombie*							ZombieGet(ZombieID theZombieID);
	Zombie*							ZombieTryToGet(ZombieID theZombieID);
	void							DrawDebugObjectRects(Sexy::Graphics* g);
	void							UpdateIce();
	int								GetIceZPos(int theRow);
	bool							CanAddBobSled();
	void							ShakeBoard(int theShakeAmountX, int theShakeAmountY);
	int								CountUntriggerLawnMowers();
	bool							IterateZombies(Zombie*& theZombie);
	bool							IteratePlants(Plant*& thePlant);
	bool							IterateProjectiles(Projectile*& theProjectile);
	bool							IterateCoins(Coin*& theCoin);
	bool							IterateLawnMowers(LawnMower*& theLawnMower);
	bool							IterateParticles(TodParticleSystem*& theParticle);
	bool							IterateReanimations(Reanimation*& theReanimation);
	bool							IterateGridItems(GridItem*& theGridItem);
	Zombie*							AddZombieInRow(ZombieType theZombieType, int theRow, int theFromWave);
	bool							IsPoolSquare(int theGridX, int theGridY);
	void							PickZombieWaves();
	void							StopAllZombieSounds();
	bool							HasLevelAwardDropped();
	void							UpdateProgressMeter();
	void							DrawUIBottom(Sexy::Graphics* g);
	void							DrawUITop(Sexy::Graphics* g);
	Zombie*							ZombieHitTest(int theMouseX, int theMouseY);
	void							KillAllPlantsInRadius(int theX, int theY, int theRadius);
	Plant*							GetPumpkinAt(int theGridX, int theGridY);
	Plant*							GetFlowerPotAt(int theGridX, int theGridY);
	static bool						CanZombieSpawnOnLevel(ZombieType theZombieType, int theLevel);
	bool							IsZombieWaveDistributionOk();
	void							PickBackground();
	void							InitZombieWaves();
	void							InitSurvivalStage();
	static int						MakeRenderOrder(RenderLayer theRenderLayer, int theRow, int theLayerOffset);
	void							UpdateGame();
	void							InitZombieWavesForLevel(int theForLevel);
	unsigned int					SeedNotRecommendedForLevel(SeedType theSeedType);
	void							DrawTopRightUI(Sexy::Graphics* g);
	void							DrawFog(Sexy::Graphics* g);
	void							UpdateFog();
	int								LeftFogColumn();
	static bool						IsZombieTypePoolOnly(ZombieType theZombieType);
	void							DropLootPiece(int thePosX, int thePosY, int theDropFactor);
	void							UpdateLevelEndSequence();
	LawnMower*						GetBottomLawnMower();
	bool							CanDropLoot();
	ZombieType						GetIntroducedZombieType();
	void							PickSpecialGraveStone();
	float							GetPosYBasedOnRow(float thePosX, int theRow);
	void							NextWaveComing();
	bool							BungeeIsTargetingCell(int theGridX, int theGridY);
	int								PlantingPixelToGridX(int theX, int theY, SeedType theSeedType);
	int								PlantingPixelToGridY(int theX, int theY, SeedType theSeedType);
	Plant*							FindUmbrellaPlant(int theGridX, int theGridY);
	void							SetTutorialState(TutorialState theTutorialState);
	void							DoFwoosh(int theRow);
	void							UpdateFwoosh();
	Plant*							SpecialPlantHitTest(int x, int y);
	void							UpdateMousePosition();
	Plant*							ToolHitTestHelper(HitResult* theHitResult);
	Plant*							ToolHitTest(int theX, int theY);
	bool							CanAddGraveStoneAt(int theGridX, int theGridY);
	void							UpdateGridItems();
	GridItem*						AddAGraveStone(int theGridX, int theGridY);
	int								GetSurvivalFlagsCompleted();
	bool							HasProgressMeter();
	void							UpdateCursor();
	void							UpdateTutorial();
	SeedType						GetSeedTypeInCursor();
	int								CountPlantByType(SeedType theSeedType);
	bool							PlantingRequirementsMet(SeedType theSeedType);
	bool							HasValidCobCannonSpot();
	bool							IsValidCobCannonSpot(int theGridX, int theGridY);
	bool							IsValidCobCannonSpotHelper(int theGridX, int theGridY);
	void							MouseDownCobcannonFire(int x, int y, int theClickCount);
	void							KillAllZombiesInRadius(int theRow, int theX, int theY, int theRadius, int theRowRange, bool theBurn, int theDamageRangeFlags);
	int								GetSeedBankExtraWidth();
	bool							IsFlagWave(int theWaveNumber);
	void							DrawHouseDoorTop(Sexy::Graphics* g);
	void							DrawHouseDoorBottom(Sexy::Graphics* g);
	Zombie*							GetBossZombie();
	bool							HasConveyorBeltSeedBank();
	bool							StageHasRoof();
	void							SpawnZombiesFromPool();
	void							SpawnZombiesFromSky();
	void							PickUpTool(GameObjectType theObjectType);
	void							TutorialArrowShow(int theX, int theY);
	void							TutorialArrowRemove();
	int								CountCoinsBeingCollected();
	void							BungeeDropZombie(BungeeDropGrid* theBungeeDropGrid, ZombieType theZombieType);
	void							SetupBungeeDrop(BungeeDropGrid* theBungeeDropGrid);
	void							PutZombieInWave(ZombieType theZombieType, int theWaveNumber, ZombiePicker* theZombiePicker);
	void							PutInMissingZombies(int theWaveNumber, ZombiePicker* theZombiePicker);
	Sexy::Rect						GetShovelButtonRect();
	void							GetZenButtonRect(GameObjectType theObjectType, Sexy::Rect& theRect);
	Plant*							NewPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	void							DoPlantingEffects(int theGridX, int theGridY, Plant* thePlant);
	bool							IsFinalSurvivalStage();
	void							SurvivalSaveScore();
	int								CountZombiesOnScreen();
	int								GetNumWavesPerSurvivalStage();
	int								GetLevelRandSeed();
	void							AddBossRenderItem(RenderItem* theRenderList, int& theCurRenderItem, Zombie* theBossZombie);
	GridItem*						GetCraterAt(int theGridX, int theGridY);
	GridItem*						GetGraveStoneAt(int theGridX, int theGridY);
	GridItem*						GetLadderAt(int theGridX, int theGridY);
	GridItem*						AddALadder(int theGridX, int theGridY);
	GridItem*						AddACrater(int theGridX, int theGridY);
	void							InitLawnMowers();
	bool							IsPlantInCursor();
	void							HighlightPlantsForMouse(int theMouseX, int theMouseY);
	void							ClearFogAroundPlant(Plant* thePlant, int theSize);
	void							RemoveParticleByType(ParticleEffect theEffectType);
	GridItem*						GetScaryPotAt(int theGridX, int theGridY);
	void							PuzzleSaveStreak();
	void							ClearAdviceImmediately();
	bool							IsFinalScaryPotterStage();
	void							DisplayAdviceAgain(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex);
	GridItem*						GetSquirrelAt(int theGridX, int theGridY);
	GridItem*						GetZenToolAt(int theGridX, int theGridY);
	bool							IsPlantInGoldWateringCanRange(int theMouseX, int theMouseY, Plant* thePlant);
	bool							StageHasZombieWalkInFromRight();
	void							PlaceRake();
	GridItem*						GetRake();
	bool							IsScaryPotterDaveTalking();
	Zombie*							GetWinningZombie();
	void							ResetFPSStats();
	int								CountEmptyPotsOrLilies(SeedType theSeedType);
	GridItem*						GetGridItemAt(GridItemType theGridItemType, int theGridX, int theGridY);
	bool							ProgressMeterHasFlags();
	bool							IsLastStandFinalStage();
	int								GetNumWavesPerFlag();
	int								GetCurrentPlantCost(SeedType theSeedType, SeedType theImitaterType);
	bool							PlantUsesAcceleratedPricing(SeedType theSeedType);
	void							FreezeEffectsForCutscene(bool theFreeze);
	void							LoadBackgroundImages();
	bool							CanUseGameObject(GameObjectType theGameObject);
	void							SetMustacheMode(bool theEnableMustache);
	int								CountCoinByType(CoinType theCoinType);
	void							SetSuperMowerMode(bool theEnableSuperMower);
	void							DrawZenWheelBarrowButton(Sexy::Graphics* g, int theOffsetY);
	void							DrawZenButtons(Sexy::Graphics* g);
	void							OffsetYForPlanting(int& theY, SeedType theSeedType);
	void							SetDanceMode(bool theEnableDance);
	void							SetFutureMode(bool theEnableFuture);
	void							SetPinataMode(bool theEnablePinata);
	void							SetDaisyMode(bool theEnableDaisy);
	void							SetSukhbirMode(bool theEnableSukhbir);
	bool							MouseHitTestPlant(int x, int y, HitResult* theHitResult);
	
	Reanimation*					CreateRakeReanim(float theRakeX, float theRakeY, int theRenderOrder);
	void							CompleteEndLevelSequenceForSaving();
	void							RemoveZombiesForRepick();
	int								GetGraveStonesCount();
	// 【性能】僵尸按行分桶查询（投射物碰撞用，语义与全池遍历一致）
	int							GetRowZombieCount(int theRow);
	Zombie*							GetRowZombie(int theRow, int theIndex);
	int							GetBossZombieCount();
	Zombie*							GetBossZombieByIndex(int theIndex);

	bool							IsSurvivalStageWithRepick();
	bool							IsLastStandStageWithRepick();
	void							DoTypingCheck(Sexy::KeyCode theKey);
	int								CountZombieByType(ZombieType theZombieType);
	static bool						IsZombieTypeSpawnedOnly(ZombieType theZombieType);
};
extern bool gShownMoreSunTutorial;

int									GetRectOverlap(const Sexy::Rect& rect1, const Sexy::Rect& rect2);
bool								GetCircleRectOverlap(int theCircleX, int theCircleY, int theRadius, const Sexy::Rect& theRect);
void								BoardInitForPlayer();

#endif // __BOARD_H__