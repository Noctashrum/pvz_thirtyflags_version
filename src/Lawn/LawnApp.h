#ifndef __LAWNAPP_H__
#define __LAWNAPP_H__

#include "Zombie.h"
#include "SeedPacket.h"
#include "../TodLib/Trail.h"
#include "../TodLib/TodFoley.h"
#include "../TodLib/Reanimator.h"
#include "../TodLib/TodParticle.h"
#include "../TodLib/TodStringFile.h"
#include "../SexyAppFramework/SexyApp.h"

class Board;
class GameSelector;
class ChallengeDefinition;
class SeedChooserScreen;
class AwardScreen;
class CreditScreen;
class PoolEffect;
class ZenGarden;
class PottedPlant;
class EffectSystem;
class ReanimatorCache;
class ProfileMgr;
class PlayerInfo;
class Music;
class TitleScreen;
class PopDRMComm;
class ChallengeScreen;
class StoreScreen;
class AlmanacDialog;
class TypingCheck;
namespace Sexy
{
	class Dialog;
	class Graphics;
	class ButtonWidget;
};
enum AwardType;
enum ChallengePage;

typedef std::list<Sexy::ButtonWidget*> ButtonList;
typedef std::list<Sexy::Image*> ImageList;

/* #################################################################################################### */

class LevelStats
{
public:
	int								mUnusedLawnMowers;

public:
	LevelStats() { Reset(); }
	inline void						Reset() { mUnusedLawnMowers = 0; }
};

/* #################################################################################################### */

enum GameScenes
{
	SCENE_LOADING,
	SCENE_MENU,
	SCENE_LEVEL_INTRO,
	SCENE_PLAYING,
	SCENE_ZOMBIES_WON,
	SCENE_AWARD,
	SCENE_CREDIT,
	SCENE_CHALLENGE
};

enum BoardResult
{
	BOARDRESULT_NONE,
	BOARDRESULT_WON,
	BOARDRESULT_LOST,
	BOARDRESULT_RESTART,
	BOARDRESULT_QUIT,
	BOARDRESULT_QUIT_APP,
	BOARDRESULT_CHEAT
};

enum CrazyDaveState
{
	CRAZY_DAVE_OFF,
	CRAZY_DAVE_ENTERING,
	CRAZY_DAVE_LEAVING,
	CRAZY_DAVE_IDLING,
	CRAZY_DAVE_TALKING,
	CRAZY_DAVE_HANDING_TALKING,
	CRAZY_DAVE_HANDING_IDLING
};

enum TrialType
{
	TRIALTYPE_NONE,
	TRIALTYPE_STAGELOCKED
};

class LawnApp : public Sexy::SexyApp
{
public:
	Board*							mBoard;											//+0x768
	TitleScreen*					mTitleScreen;									//+0x76C
	GameSelector*					mGameSelector;									//+0x770
	SeedChooserScreen*				mSeedChooserScreen;								//+0x774
	AwardScreen*					mAwardScreen;									//+0x778
	CreditScreen*					mCreditScreen;									//+0x77C
	ChallengeScreen*				mChallengeScreen;								//+0x780
	TodFoley*						mSoundSystem;									//+0x784
	ButtonList						mControlButtonList;								//+0x788
	ImageList						mCreatedImageList;								//+0x794
	std::string						mReferId;										//+0x7A0
	std::string						mRegisterLink;									//+0x7BC
	std::string						mMod;											//+0x7D8
	bool							mRegisterResourcesLoaded;						//+0x7F4
	bool							mTodCheatKeys;									//+0x7F5
	GameMode						mGameMode;										//+0x7F8
	GameScenes						mGameScene;										//+0x7FC
	bool							mLoadingZombiesThreadCompleted;					//+0x800
	bool							mFirstTimeGameSelector;							//+0x801
	int								mGamesPlayed;									//+0x804
	int								mMaxExecutions;									//+0x808
	int								mMaxPlays;										//+0x80C
	int								mMaxTime;										//+0x810
	bool							mEasyPlantingCheat;								//+0x814
	PoolEffect*						mPoolEffect;									//+0x818
	ZenGarden*						mZenGarden;										//+0x81C
	EffectSystem*					mEffectSystem;									//+0x820
	ReanimatorCache*				mReanimatorCache;								//+0x824
	ProfileMgr*						mProfileMgr;									//+0x828
	PlayerInfo*						mPlayerInfo;									//+0x82C
	LevelStats*						mLastLevelStats;								//+0x830
	bool							mCloseRequest;									//+0x834
	int								mAppCounter;									//+0x838
	Music*							mMusic;											//+0x83C
	ReanimationID					mCrazyDaveReanimID;								//+0x840
	CrazyDaveState					mCrazyDaveState;								//+0x844
	int								mCrazyDaveBlinkCounter;							//+0x848
	ReanimationID					mCrazyDaveBlinkReanimID;						//+0x84C
	int								mCrazyDaveMessageIndex;							//+0x850
	SexyString						mCrazyDaveMessageText;							//+0x854
	int								mAppRandSeed;									//+0x870
	HICON							mBigArrowCursor;								//+0x874
	PopDRMComm*						mDRM;											//+0x878
	int								mSessionID;										//+0x87C
	int								mPlayTimeActiveSession;							//+0x880
	int								mPlayTimeInactiveSession;						//+0x884
	BoardResult						mBoardResult;									//+0x888
	bool							mSawYeti;										//+0x88C
	TypingCheck*					mKonamiCheck;									//+0x890
	TypingCheck*					mMustacheCheck;									//+0x894
	TypingCheck*					mMoustacheCheck;								//+0x898
	TypingCheck*					mSuperMowerCheck;								//+0x89C
	TypingCheck*					mSuperMowerCheck2;								//+0x8A0
	TypingCheck*					mFutureCheck;									//+0x8A4
	TypingCheck*					mPinataCheck;									//+0x8A8
	TypingCheck*					mDanceCheck;									//+0x8AC
	TypingCheck*					mDaisyCheck;									//+0x8B0
	TypingCheck*					mSukhbirCheck;									//+0x8B4

	bool							mMustacheMode;									//+0x8B8
	bool							mSuperMowerMode;								//+0x8B9
	bool							mFutureMode;									//+0x8BA
	bool							mPinataMode;									//+0x8BB
	bool							mDanceMode;										//+0x8BC
	bool							mDaisyMode;										//+0x8BD
	bool							mSukhbirMode;									//+0x8BE
	TrialType						mTrialType;										//+0x8C0
	bool							mDebugTrialLocked;								//+0x8C4
	bool							mMuteSoundsForCutscene;							//+0x8C5

public:
	LawnApp();
	virtual ~LawnApp();

	bool							KillNewOptionsDialog();
	virtual void					GotFocus();
	virtual void					LostFocus();
	virtual void					InitHook();
	virtual void					WriteToRegistry();
	virtual void					ReadFromRegistry();
	virtual void					LoadingThreadProc();
	virtual void					LoadingCompleted();
	virtual void					LoadingThreadCompleted();
	virtual void					URLOpenFailed(const std::string& theURL);
	virtual void					URLOpenSucceeded(const std::string& theURL);
	virtual bool					OpenURL(const std::string& theURL, bool shutdownOnOpen);
	virtual bool					DebugKeyDown(int theKey);
	virtual void					HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue);
	void							ConfirmQuit();
	void							ConfirmCheckForUpdates() { ; }
	void							CheckForUpdates() { ; }
	void							DoUserDialog();
	void							FinishUserDialog(bool isYes);
	void							DoCreateUserDialog();
	void							DoCheatDialog();
	void							FinishCheatDialog(bool isYes);
	void							FinishCreateUserDialog(bool isYes);
	void							DoConfirmDeleteUserDialog(const SexyString& theName);
	void							FinishConfirmDeleteUserDialog(bool isYes);
	void							DoRenameUserDialog(const SexyString& theName);
	void							FinishRenameUserDialog(bool isYes);
	void							FinishNameError(int theId);
	void							FinishRestartConfirmDialog();
	void							DoConfirmSellDialog(const SexyString& theMessage);
	void							DoConfirmPurchaseDialog(const SexyString& theMessage);
	void							FinishTimesUpDialog();
	void							KillBoard();
	void							MakeNewBoard();
	void							StartPlaying();
	bool							TryLoadGame();
	void							NewGame();
	void							PreNewGame(GameMode theGameMode, bool theLookForSavedGame);
	void							ShowGameSelector();
	void							KillGameSelector();
	void							ShowAwardScreen(AwardType theAwardType);
	void							KillAwardScreen();
	void							ShowSeedChooserScreen();
	void							KillSeedChooserScreen();
	void							DoHighScoreDialog();
	void							DoBackToMain();
	void							DoConfirmBackToMain();
	void							DoNewOptions(bool theFromGameSelector);
	void							DoRegister();
	void							DoRegisterError();
	bool							CanDoRegisterDialog();
	bool							WriteCurrentUserConfig();
	void							DoNeedRegisterDialog();
	void							DoContinueDialog();
	void							DoPauseDialog();
	void							FinishModelessDialogs();
	virtual Sexy::Dialog*			DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual Sexy::Dialog*			DoDialogDelay(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual void					Shutdown();
	virtual void					Init();
	virtual void					Start();
	virtual Sexy::Dialog*			NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual bool					KillDialog(int theDialogId);
	virtual void					ModalOpen();
	virtual void					ModalClose();
	virtual void					PreDisplayHook();
	virtual bool					ChangeDirHook(const char* theIntendedPath);
	virtual bool					NeedRegister();
	virtual void					UpdateRegisterInfo();
	virtual void					ButtonPress(int theId);
	virtual void					ButtonDepress(int theId);
	virtual void					UpdateFrames();
	virtual bool					UpdateApp();
	bool							IsAdventureMode();
	bool							IsSurvivalMode();

	// 【三十旗改版】把模块侧的关键节点写进 Release\thirtyflags.log（Release 无断言、无日志）。
	void							TfLog(const char* theFormat, ...);
	bool							IsContinuousChallenge();
	bool							IsArtChallenge();
	bool							NeedPauseGame();
	virtual void					ShowResourceError(bool doExit = false);
	void							ToggleSlowMo();
	void							ToggleFastMo();
	void							PlayFoley(FoleyType theFoleyType);
	void							PlayFoleyPitch(FoleyType theFoleyType, float thePitch);
	void							PlaySample(int theSoundNum);
	void							FastLoad(GameMode theGameMode);
	static SexyString				GetStageString(int theLevel);
	void							KillChallengeScreen();
	void							ShowChallengeScreen(ChallengePage thePage);
	ChallengeDefinition&			GetCurrentChallengeDef();
	void							CheckForGameEnd();
	virtual void					CloseRequestAsync();
	bool							IsChallengeWithoutSeedBank();
	AlmanacDialog*					DoAlmanacDialog(SeedType theSeedType = SeedType::SEED_NONE, ZombieType theZombieType = ZombieType::ZOMBIE_INVALID);
	bool							KillAlmanacDialog();
	int								GetSeedsAvailable();
	Reanimation*					AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType);
	TodParticleSystem*				AddTodParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect);
	ParticleSystemID				ParticleGetID(TodParticleSystem* theParticle);
	TodParticleSystem*				ParticleGet(ParticleSystemID theParticleID);
	TodParticleSystem*				ParticleTryToGet(ParticleSystemID theParticleID);
	ReanimationID					ReanimationGetID(Reanimation* theReanimation);
	Reanimation*					ReanimationGet(ReanimationID theReanimationID);
	Reanimation*					ReanimationTryToGet(ReanimationID theReanimationID);
	void							RemoveReanimation(ReanimationID theReanimationID);
	void							RemoveParticle(ParticleSystemID theParticleID);
	StoreScreen*					ShowStoreScreen();
	void							KillStoreScreen();
	bool							HasSeedType(SeedType theSeedType);
	bool							SeedTypeAvailable(SeedType theSeedType);
	void							EndLevel();
	inline bool						IsIceDemo() { return false; }
	bool							IsShovelLevel();
	bool							IsWallnutBowlingLevel();
	bool							IsMiniBossLevel();
	bool							IsSlotMachineLevel();
	bool							IsLittleTroubleLevel();
	bool							IsStormyNightLevel();
	bool							IsFinalBossLevel();
	bool							IsBungeeBlitzLevel();
	static SeedType					GetAwardSeedForLevel(int theLevel);
	SexyString						GetCrazyDaveText(int theMessageIndex);
	bool							CanShowAlmanac();
	bool							IsNight();
	bool							CanShowStore();
	bool							HasBeatenChallenge(GameMode theGameMode);
	PottedPlant*					GetPottedPlantByIndex(int thePottedPlantIndex);
	static bool						IsSurvivalNormal(GameMode theGameMode);
	static bool						IsSurvivalHard(GameMode theGameMode);
	static bool						IsSurvivalEndless(GameMode theGameMode);
	bool							HasFinishedAdventure();
	bool							IsFirstTimeAdventureMode();
	bool							CanSpawnYetis();
	void							CrazyDaveEnter();
	void							UpdateCrazyDave();
	void							CrazyDaveTalkIndex(int theMessageIndex);
	void							CrazyDaveTalkMessage(const SexyString& theMessage);
	void							CrazyDaveLeave();
	void							DrawCrazyDave(Sexy::Graphics* g);
	void							CrazyDaveDie();
	void							CrazyDaveStopTalking();
	void							PreloadForUser();
	int								GetNumPreloadingTasks();
	int								LawnMessageBox(int theDialogId, const SexyChar* theHeaderName, const SexyChar* theLinesName, const SexyChar* theButton1Name, const SexyChar* theButton2Name, int theButtonMode);
	virtual void					EnforceCursor();
	void							ShowCreditScreen();
	void							KillCreditScreen();
	static SexyString				Pluralize(int theCount, const SexyChar* theSingular, const SexyChar* thePlural);
	int								GetNumTrophies(ChallengePage thePage);
	bool							EarnedGoldTrophy();
	inline bool						IsRegistered() { return false; }
	inline bool						IsExpired() { return false; }
	inline bool						IsDRMConnected() { return false; }
	bool							IsScaryPotterLevel();
	static bool						IsEndlessScaryPotter(GameMode theGameMode);
	bool							IsSquirrelLevel();
	bool							IsIZombieLevel();
	bool							CanShowZenGarden();
	static SexyString				GetMoneyString(int theAmount);
	bool							AdvanceCrazyDaveText();
	bool							IsWhackAZombieLevel();
	void							BetaSubmit(bool theAskForComments);
	void							BetaRecordLevelStats();
	void							UpdatePlayTimeStats();
	void							BetaAddFile(std::list<std::string>& theUploadFileList, std::string theFileName, std::string theShortName);
	bool							CanPauseNow();
	bool							IsPuzzleMode();
	bool							IsChallengeMode();
	static bool						IsEndlessIZombie(GameMode theGameMode);
	void							CrazyDaveDoneHanding();
	static inline SexyString		GetCurrentLevelName() { return _S("Unknown"); }
	int								TrophiesNeedForGoldSunflower();
	int								GetCurrentChallengeIndex();
	void							LoadGroup(const char* theGroupName, int theGroupAveMsToLoad);
	void							TraceLoadGroup(const char* theGroupName, int theGroupTime, int theTotalGroupWeigth, int theTaskWeight);
	void							CrazyDaveStopSound();
	bool							IsTrialStageLocked();
	void							FinishZenGardenToturial();
	bool							UpdatePlayerProfileForFinishingLevel();
	bool							SaveFileExists();
	bool							CanDoPinataMode();
	bool							CanDoDanceMode();
	bool							CanDoDaisyMode();
	virtual void					SwitchScreenMode(bool wantWindowed, bool is3d, bool force = false);
	static void						CenterDialog(Sexy::Dialog* theDialog, int theWidth, int theHeight);
};

SexyString							LawnGetCurrentLevelName();
bool								LawnGetCloseRequest();
bool								LawnHasUsedCheatKeys();
void								BetaSubmitFunc();

extern bool							gIsPartnerBuild;
extern bool							gFastMo;			//0x6A9EAB
extern bool							gSlowMo;			//0x6A9EAA
extern LawnApp*						gLawnApp;			//0x6A9EC0
extern int							gSlowMoCounter;		//0x6A9EC4

/* #################################################################################################### */

enum FoleyType
{
	FOLEY_SUN,
	FOLEY_SPLAT,
	FOLEY_LAWNMOWER,
	FOLEY_THROW,
	FOLEY_SPAWN_SUN,
	FOLEY_CHOMP,
	FOLEY_CHOMP_SOFT,
	FOLEY_PLANT,
	FOLEY_USE_SHOVEL,
	FOLEY_DROP,
	FOLEY_BLEEP,
	FOLEY_GROAN,
	FOLEY_BRAINS,
	FOLEY_SUKHBIR,
	FOLEY_JACKINTHEBOX,
	FOLEY_ART_CHALLENGE,
	FOLEY_ZAMBONI,
	FOLEY_THUNDER,
	FOLEY_FROZEN,
	FOLEY_ZOMBIESPLASH,
	FOLEY_BOWLINGIMPACT,
	FOLEY_SQUISH,
	FOLEY_TIRE_POP,
	FOLEY_EXPLOSION,
	FOLEY_SLURP,
	FOLEY_LIMBS_POP,
	FOLEY_POGO_ZOMBIE,
	FOLEY_SNOW_PEA_SPARKLES,
	FOLEY_ZOMBIE_FALLING,
	FOLEY_PUFF,
	FOLEY_FUME,
	FOLEY_COIN,
	FOLEY_KERNEL_SPLAT,
	FOLEY_DIGGER,
	FOLEY_JACK_SURPRISE,
	FOLEY_VASE_BREAKING,
	FOLEY_POOL_CLEANER,
	FOLEY_BASKETBALL,
	FOLEY_IGNITE,
	FOLEY_FIREPEA,
	FOLEY_THUMP,
	FOLEY_SQUASH_HMM,
	FOLEY_MAGNETSHROOM,
	FOLEY_BUTTER,
	FOLEY_BUNGEE_SCREAM,
	FOLEY_BOSS_EXPLOSION_SMALL,
	FOLEY_SHIELD_HIT,
	FOLEY_SWING,
	FOLEY_BONK,
	FOLEY_RAIN,
	FOLEY_DOLPHIN_BEFORE_JUMPING,
	FOLEY_DOLPHIN_APPEARS,
	FOLEY_PLANT_WATER,
	FOLEY_ZOMBIE_ENTERING_WATER,
	FOLEY_GRAVEBUSTERCHOMP,
	FOLEY_CHERRYBOMB,
	FOLEY_JALAPENO_IGNITE,
	FOLEY_REVERSE_EXPLOSION,
	FOLEY_PLASTIC_HIT,
	FOLEY_WINMUSIC,
	FOLEY_BALLOONINFLATE,
	FOLEY_BIGCHOMP,
	FOLEY_MELONIMPACT,
	FOLEY_PLANTGROW,
	FOLEY_SHOOP,
	FOLEY_JUICY,
	FOLEY_NEWSPAPER_RARRGH,
	FOLEY_NEWSPAPER_RIP,
	FOLEY_FLOOP,
	FOLEY_COFFEE,
	FOLEY_LOW_GROAN,
	FOLEY_PRIZE,
	FOLEY_YUCK,
	FOLEY_UMBRELLA,
	FOLEY_GRASSSTEP,
	FOLEY_SHOVEL,
	FOLEY_COB_LAUNCH,
	FOLEY_WATERING,
	FOLEY_POLEVAULT,
	FOLEY_GRAVESTONE_RUMBLE,
	FOLEY_DIRT_RISE,
	FOLEY_FERTILIZER,
	FOLEY_PORTAL,
	FOLEY_WAKEUP,
	FOLEY_BUGSPRAY,
	FOLEY_SCREAM,
	FOLEY_PAPER,
	FOLEY_MONEYFALLS,
	FOLEY_IMP,
	FOLEY_HYDRAULIC_SHORT,
	FOLEY_HYDRAULIC,
	FOLEY_GARGANTUDEATH,
	FOLEY_CERAMIC,
	FOLEY_BOSS_BOULDER_ATTACK,
	FOLEY_CHIME,
	FOLEY_CRAZY_DAVE_SHORT,
	FOLEY_CRAZY_DAVE_LONG,
	FOLEY_CRAZY_DAVE_EXTRA_LONG,
	FOLEY_CRAZY_DAVE_CRAZY,
	FOLEY_PHONOGRAPH,
	FOLEY_DANCER,
	FOLEY_FINAL_FANFARE,
	FOLEY_CRAZY_DAVE_SCREAM,
	FOLEY_CRAZY_DAVE_SCREAM_2,
	NUM_FOLEY
};

extern FoleyParams					gLawnFoleyParamArray[(int)FoleyType::NUM_FOLEY];			//0x69FAD0
extern ParticleParams				gLawnParticleArray[(int)ParticleEffect::NUM_PARTICLES];		//0x6A0FF0
extern ReanimationParams			gLawnReanimationArray[(int)ReanimationType::NUM_REANIMS];	//0x6A1340
extern TrailParams					gLawnTrailArray[(int)TrailType::NUM_TRAILS];				//0x6A19F4

extern int							gLawnStringFormatCount;
extern TodStringListFormat			gLawnStringFormats[12];  //0x6A5010

#endif	// __LAWNAPP_H__