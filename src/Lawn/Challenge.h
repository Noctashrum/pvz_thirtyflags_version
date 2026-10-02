#ifndef __CHALLENGE_H__
#define __CHALLENGE_H__

#include "../TodLib/FilterEffect.h"
#include "../SexyAppFramework/Graphics.h"

#define BEGHOULED_MAX_GRIDSIZEX		8
#define BEGHOULED_MAX_GRIDSIZEY		5
#define ART_CHALLEGE_SIZE_X			MAX_GRID_SIZE_X
#define MAX_PICK_GRID_SIZE			50

constexpr const int		LAST_STAND_FLAGS			= 5;
constexpr const int		SURVIVAL_NORMAL_FLAGS		= 5;
constexpr const int		SURVIVAL_HARD_FLAGS			= 10;

class LawnApp;
class Board;
class Plant;
class Zombie;
class GridItem;
class SeedPacket;
class HitResult;
struct TodWeightedGridArray;
enum SeedType;
enum ReanimationID;

enum BeghouledUpgrade
{
	BEGHOULED_UPGRADE_REPEATER,
	BEGHOULED_UPGRADE_FUMESHROOM,
	BEGHOULED_UPGRADE_TALLNUT,
	NUM_BEGHOULED_UPGRADES
};

struct BeghouledBoardState
{
	SeedType				mSeedType[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];
};

enum ChallengeState
{
	STATECHALLENGE_NORMAL,
	STATECHALLENGE_BEGHOULED_MOVING,
	STATECHALLENGE_BEGHOULED_FALLING,
	STATECHALLENGE_BEGHOULED_NO_MATCHES,
	STATECHALLENGE_SLOT_MACHINE_ROLLING,
	STATECHALLENGE_STORM_FLASH_1,
	STATECHALLENGE_STORM_FLASH_2,
	STATECHALLENGE_STORM_FLASH_3,
	STATECHALLENGE_ZEN_FADING,
	STATECHALLENGE_SCARY_POTTER_MALLETING,
	STATECHALLENGE_LAST_STAND_ONSLAUGHT,
	STATECHALLENGE_TREE_JUST_GREW,
	STATECHALLENGE_TREE_GIVE_WISDOM,
	STATECHALLENGE_TREE_WAITING_TO_BABBLE,
	STATECHALLENGE_TREE_BABBLING
};

class Challenge
{
public:
	LawnApp*				mApp;												//+0x0
	Board*					mBoard;												//+0x4
	bool					mBeghouledMouseCapture;								//+0x8
	int						mBeghouledMouseDownX;								//+0xC
	int						mBeghouledMouseDownY;								//+0x10
	bool					mBeghouledEated[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];  //+0x14
	bool					mBeghouledPurcasedUpgrade[NUM_BEGHOULED_UPGRADES];  //+0x4A
	int						mBeghouledMatchesThisMove;							//+0x50
	ChallengeState			mChallengeState;									//+0x54
	int						mChallengeStateCounter;								//+0x58
	int						mConveyorBeltCounter;								//+0x5C
	int						mChallengeScore;									//+0x60
	bool					mShowBowlingLine;									//+0x64
	SeedType				mLastConveyorSeedType;								//+0x68
	int						mSurvivalStage;										//+0x6C
	int						mSlotMachineRollCount;								//+0x70
	ReanimationID			mReanimChallenge;									//+0x74
	ReanimationID			mReanimClouds[6];									//+0x78
	int						mCloudsCounter[6];									//+0x90
	int						mChallengeGridX;									//+0xA8
	int						mChallengeGridY;									//+0xAC
	int						mScaryPotterPots;									//+0xB0
	int						mRainCounter;										//+0xB4
	int						mTreeOfWisdomTalkIndex;								//+0xB8

public:
	Challenge();

	void					StartLevel();
	void					BeghouledPopulateBoard();
	void					LoadBeghouledBoardState(BeghouledBoardState* theState);
	SeedType				BeghouledPickSeed(int theGridX, int theGridY, BeghouledBoardState* theBoardState, bool theAllowMatches);
	bool					BeghouledBoardHasMatch(BeghouledBoardState* theBoardState);
	SeedType				BeghouledGetPlantAt(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	int						BeghouledVerticalMatchLength(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	int						BeghouledHorizontalMatchLength(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	void					BeghouledDragStart(int x, int y);
	void					BeghouledDragUpdate(int x, int y);
	void					BeghouledDragCancel();
	bool					MouseMove(int x, int y);
	bool					MouseDown(int x, int y, int theClickCount, HitResult* theHitResult);
	bool					MouseUp(int x, int y);
	void					ClearCursor();
	void					BeghouledRemoveHorizontalMatch(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	void					BeghouledRemoveVerticalMatch(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	void					BeghouledRemoveMatches(BeghouledBoardState* theBoardState);
	void					Update();
	void					UpdateBeghouled();
	bool					UpdateBeghouledPlant(Plant* thePlant);
	void					BeghouledFallIntoSquare(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	void					BeghouledMakePlantsFall(BeghouledBoardState* theBoardState);
	void					ZombieAtePlant(Zombie* theZombie, Plant* thePlant);
	void					DrawBackdrop(Sexy::Graphics* g);
	void					DrawArtChallenge(Sexy::Graphics* g);
	void					CheckForCompleteArtChallenge(int theGridX, int theGridY);
	SeedType				GetArtChallengeSeed(int theGridX, int theGridY);
	void					PlantAdded(Plant* thePlant);
	PlantingReason			CanPlantAt(int theGridX, int theGridY, SeedType theSeedType);
	void					DrawBeghouled(Sexy::Graphics* g);
	bool					BeghouledIsValidMove(int theFromX, int theFromY, int theToX, int theToY, BeghouledBoardState* theBoardState);
	bool					BeghouledCheckForPossibleMoves(BeghouledBoardState* theBoardState);
	void					BeghouledCheckStuckState();
	void					InitZombieWavesSurvival();
	void					InitZombieWavesFromList(ZombieType* theZombieList, int theListLength);
	void					InitZombieWaves();
	Sexy::Rect				SlotMachineGetHandleRect();
	void					UpdateSlotMachine();
	void					DrawSlotMachine(Sexy::Graphics* g);
	bool					UpdateToolTip(int theX, int theY);
	void					WhackAZombieSpawning();
	bool					UpdateZombieSpawning();
	void					BeghouledClearCrater(int theCount);
	void					MouseDownWhackAZombie(int theX, int theY);
	void					DrawStormNight(Sexy::Graphics* g);
	void					UpdateStormyNight();
	void					InitLevel();
	void					SpawnZombieWave();
	void					GraveDangerSpawnRandomGrave();
	void					GraveDangerSpawnGraveAt(int theGridX, int theGridY);
	void					SpawnLevelAward(int theGridX, int theGridY);
	void					BeghouledScore(int theGridX, int theGridY, int theNumPlants, bool theIsHorizontal);
	void					DrawStormFlash(Sexy::Graphics* g, int theTime, int theMaxAmount);
	void					UpdateRainingSeeds();
	void					PlayBossEnter();
	void					UpdateConveyorBelt();
	void					PortalStart();
	void					UpdatePortalCombat();
	GridItem*				GetOtherPortal(GridItem* thePortal);
	void					UpdatePortal(GridItem* thePortal);
	float					PortalCombatRowSpawnWeight(int theGridY);
	bool					CanTargetZombieWithPortals(Plant* thePlant, Zombie* theZombie);
	GridItem*				GetPortalToRight(int theGridX, int theGridY);
	GridItem*				GetPortalAt(int theGridX, int theGridY);
	void					MoveAPortal();
	int						GetPortalDistanceToMower(int theGridY);
	GridItem*				GetPortalToLeft(int theGridX, int theGridY);
	void					BeghouledPacketClicked(SeedPacket* theSeedPacket);
	void					BeghouledShuffle();
	bool					BeghouledCanClearCrater();
	void					BeghouledUpdateCraters();
	Zombie*					ZombiquariumSpawnSnorkle();
	void					ZombiquariumPacketClicked(SeedPacket* theSeedPacket);
	void					ZombiquariumMouseDown(int x, int y);
	void					ZombiquariumDropBrain(int x, int y);
	void					ZombiquariumUpdate();
	void					ShovelAddWallnuts();
	void					ScaryPotterPlacePot(ScaryPotType theScaryPotType, ZombieType theZombieType, SeedType theSeedType, int theCount, TodWeightedGridArray* theGridArray, int theGridArrayCount);
	void					ScaryPotterStart();
	void					ScaryPotterUpdate();
	void					ScaryPotterOpenPot(GridItem* theScaryPot);
	void					ScaryPotterJackExplode(int thePosX, int thePosY);
	bool					ScaryPotterIsCompleted();
	void					ScaryPotterChangePotType(GridItemState thePotType, int theCount);
	void					ScaryPotterPopulate();
	void					ScaryPotterDontPlaceInCol(int theCol, TodWeightedGridArray* theGridArray, int theGridArrayCount);
	void					ScaryPotterFillColumnWithPlant(int theCol, SeedType theSeedType, TodWeightedGridArray* theGridArray, int theGridArrayCount);
	void					PuzzleNextStageClear();
	void					ScaryPotterMalletPot(GridItem* theScaryPot);
	static ZombieType		IZombieSeedTypeToZombieType(SeedType theSeedType);
	static bool				IsZombieSeedType(SeedType theSeedType);
	void					IZombieMouseDownWithZombie(int theX, int theY, int theClickCount);
	void					IZombieStart();
	void					IZombiePlacePlants(SeedType theSeedType, int theCount, int theGridY = -1);
	void					IZombieUpdate();
	void					IZombieDrawPlant(Sexy::Graphics* g, Plant* thePlant);
	void					IZombieSetPlantFilterEffect(Plant* thePlant, FilterEffect theFilterEffect);
	int						ScaryPotterCountSunInPot(GridItem* theScaryPot);
	int						ScaryPotterCountPots();
	void					IZombieInitLevel();
	void					DrawRain(Sexy::Graphics* g);
	void					DrawWeather(Sexy::Graphics* g);
	void					SquirrelUpdate();
	int						SquirrelCountUncaught();
	void					SquirrelStart();
	void					SquirrelFound(GridItem* theSquirrel);
	void					SquirrelPeek(GridItem* theSquirrel);
	void					SquirrelChew(GridItem* theSquirrel);
	void					SquirrelUpdateOne(GridItem* theSquirrel);
	void					IZombieSetupPlant(Plant* thePlant);
	void					UpdateRain();
	bool					IZombieEatBrain(Zombie* theZombie);
	GridItem*				IZombieGetBrainTarget(Zombie* theZombie);
	void					IZombiePlacePlantInSquare(SeedType theSeedType, int theGridX, int theGridY = -1);
	void					AdvanceCrazyDaveDialog();
	void					BeghouledFlashPlant(int theFlashX, int theFlashY, int theFromX, int theFromY, int theToX, int theToY);
	void					BeghouledFlashAMatch();
	bool					BeghouledFlashFromBoardState(BeghouledBoardState* theBoardState, int theFromX, int theFromY, int theToX, int theToY);
	void					IZombiePlantDropRemainingSun(Plant* thePlant);
	void					IZombieSquishBrain(GridItem* theBrain);
	void					IZombieScoreBrain(GridItem* theBrain);
	void					LastStandUpdate();
	void					WhackAZombiePlaceGraves(int theGraveCount);
	bool					BeghouledTwistSquareFromMouse(int theX, int theY, int& theGridX, int& theGridY);
	bool					BeghouledTwistValidMove(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	void					BeghouledTwistMouseDown(int x, int y);
	bool					BeghouledTwistMoveCausesMatch(int theGridX, int theGridY, BeghouledBoardState* theBoardState);
	bool					BeghouledTwistFlashMatch(BeghouledBoardState* theBoardState, int theGridX, int theGridY);
	void					BeghouledCancelMatchFlashing();
	void					BeghouledStartFalling(ChallengeState theState);
	void					BeghouledFillHoles(BeghouledBoardState* theBoardState, bool theAllowMatches);
	void					BeghouledMakeStartBoard();
	void					BeghouledCreatePlants(BeghouledBoardState* theOldBoardState, BeghouledBoardState* theNewBoardState);
	void					PuzzlePhaseComplete(int theGridX, int theGridY);
	bool					PuzzleIsAwardStage();
	void					IZombiePlaceZombie(ZombieType theZombieType, int theGridX, int theGridY);
	void					WhackAZombieUpdate();
	void					LastStandCompletedStage();
	void					TreeOfWisdomUpdate();
	void					TreeOfWisdomFertilize();
	void					TreeOfWisdomInit();
	bool					TreeOfWisdomMouseOn(int theX, int theY);
	int						TreeOfWisdomGetSize();
	void					TreeOfWisdomDraw(Sexy::Graphics* g);
	void					TreeOfWisdomNextGarden();
	void					TreeOfWisdomToolUpdate(GridItem* theZenTool);
	void					TreeOfWisdomOpenStore();
	void					TreeOfWisdomLeave();
	void					TreeOfWisdomGrow();
	void					TreeOfWisdomTool(int theMouseX, int theMouseY);
	bool					TreeOfWisdomHitTest(int theX, int theY, HitResult* theHitResult);
	void					TreeOfWisdomBabble();
	void					TreeOfWisdomGiveWisdom();
	void					TreeOfWisdomSayRepeat();
	bool					TreeOfWisdomCanFeed();
};

extern SeedType gArtChallengeWallnut[MAX_GRID_SIZE_Y][ART_CHALLEGE_SIZE_X];
extern SeedType gArtChallengeSunFlower[MAX_GRID_SIZE_Y][ART_CHALLEGE_SIZE_X];
extern SeedType gArtChallengeStarFruit[MAX_GRID_SIZE_Y][ART_CHALLEGE_SIZE_X];

class ZombieAllowedLevels
{
public:
	ZombieType				mZombieType;
	int						mAllowedOnLevel[NUM_LEVELS];
};
extern int gZombieWaves[NUM_LEVELS]; //0x6A34E8
extern ZombieAllowedLevels gZombieAllowedLevels[NUM_ZOMBIE_TYPES];  //0x6A35B0

#endif