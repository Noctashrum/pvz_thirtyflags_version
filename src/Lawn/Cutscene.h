#ifndef __CUTSCENE_H__
#define __CUTSCENE_H__

class LawnApp;
class Board;
class ChallengeScreen;

class CutScene
{
public:
	LawnApp*					mApp;							//+0x0
	Board*						mBoard;							//+0x4
	int							mCutsceneTime;					//+0x8
	int							mSodTime;						//+0xC
	int							mGraveStoneTime;				//+0x10
	int							mReadySetPlantTime;				//+0x14
	int							mFogTime;						//+0x18
	int							mBossTime;						//+0x1C
	int							mCrazyDaveTime;					//+0x20
	int							mLawnMowerTime;					//+0x24
	int							mCrazyDaveDialogStart;			//+0x28
	bool						mSeedChoosing;					//+0x2C
	ReanimationID				mZombiesWonReanimID;			//+0x30
	bool						mPreloaded;						//+0x34
	bool						mPlacedZombies;					//+0x35
	bool						mPlacedLawnItems;				//+0x36
	int							mCrazyDaveCountDown;			//+0x38 【推销戴夫倒计时】
	int							mCrazyDaveLastTalkIndex;		//+0x3C 【戴夫推销的话的编号】
	bool						mUpsellHideBoard;				//+0x40 【推销隐藏关卡界面】
	ChallengeScreen*			mUpsellChallengeScreen;			//+0x44 【推销插入的小游戏界面】
	bool						mPreUpdatingBoard;				//+0x48 【场景布置阶段的关卡预更新】

public:
	CutScene();
	~CutScene();

	void						StartLevelIntro();
	void						CancelIntro();
	void						Update();
	void						AnimateBoard();
	void						StartSeedChooser();
	void						EndSeedChooser();
	int							CalcPosition(int theTimeStart, int theTimeEnd, int thePositionStart, int thePositionEnd);
	void						PlaceStreetZombies();
	void						AddGraveStoneParticles();
	void						PlaceAZombie(ZombieType theZombieType, int theGridX, int theGridY);
	bool						CanZombieGoInGridSpot(ZombieType theZombieType, int theGridX, int theGridY, bool theZombieGrid[5][5]);
	bool						IsSurvivalRepick();
	bool						IsAfterSeedChooser();
	void						AddFlowerPots();
	void						UpdateZombiesWon();
	void						StartZombiesWon();
	bool						ShowZombieWalking();
	bool						IsCutSceneOver();
	void						ZombieWonClick();
	void						MouseDown(int theX, int theY);
	void						KeyDown(Sexy::KeyCode theKey);
	void						AdvanceCrazyDaveDialog(bool theJustSkipping);
	void						ShowShovel();
	bool						CanGetPacketUpgrade();
	bool						CanGetPacketUpgrade(int theIndex);
	void						FindPlaceForStreetZombies(ZombieType theZombieType, bool theZombieGrid[5][5], int& thePosX, int& thePosY);
	void						FindAndPlaceZombie(ZombieType theZombieType, bool theZombieGrid[5][5]);
	static bool					Is2x2Zombie(ZombieType theZombieType);
	void						PreloadResources();
	bool						IsBeforePreloading();
	bool						IsShowingCrazyDave();
	bool						IsNonScrollingCutscene();
	bool						IsScrolledLeftAtStart();
	bool						IsInShovelTutorial();
	void						PlaceLawnItems();
	bool						CanGetSecondPacketUpgrade();
	int							ParseDelayTimeFromMessage();
	int							ParseTalkTimeFromMessage();
	void						ClearUpsellBoard();
	void						LoadIntroBoard();
	void						AddUpsellZombie(ZombieType theZombieType, int thePixelX, int theGridY);
	void						LoadUpsellBoardPool();
	void						LoadUpsellBoardFog();
	void						LoadUpsellChallengeScreen();
	void						LoadUpsellBoardRoof();
	void						UpdateUpsell();
	void						DrawUpsell(Sexy::Graphics* g);
	void						UpdateIntro();
	void						DrawIntro(Sexy::Graphics* g);
	bool						ShouldRunUpsellBoard();
};

#endif
