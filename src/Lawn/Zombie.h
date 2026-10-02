#ifndef __ZOMBIE_H__
#define __ZOMBIE_H__

#include "GameObject.h"

#define MAX_ZOMBIE_FOLLOWERS	4

class Plant;
class Reanimation;
class TodParticleSystem;
enum PlantID;
enum AttachmentID;
enum ReanimationID;
enum ReanimationType;
enum ReanimLoopType;
enum ParticleEffect;

class ZombieDrawPosition
{
public:
	int								mHeadX;
	int								mHeadY;
	int								mArmY;
	float							mBodyY;
	float							mImageOffsetX;
	float							mImageOffsetY;
	float							mClipHeight;
};

enum ZombieType
{
	ZOMBIE_INVALID = -1,
	ZOMBIE_NORMAL,
	ZOMBIE_FLAG,
	ZOMBIE_TRAFFIC_CONE,
	ZOMBIE_POLEVAULTER,
	ZOMBIE_PAIL,
	ZOMBIE_NEWSPAPER,
	ZOMBIE_DOOR,
	ZOMBIE_FOOTBALL,
	ZOMBIE_DANCER,
	ZOMBIE_BACKUP_DANCER,
	ZOMBIE_DUCKY_TUBE,
	ZOMBIE_SNORKEL,
	ZOMBIE_ZAMBONI,
	ZOMBIE_BOBSLED,
	ZOMBIE_DOLPHIN_RIDER,
	ZOMBIE_JACK_IN_THE_BOX,
	ZOMBIE_BALLOON,
	ZOMBIE_DIGGER,
	ZOMBIE_POGO,
	ZOMBIE_YETI,
	ZOMBIE_BUNGEE,
	ZOMBIE_LADDER,
	ZOMBIE_CATAPULT,
	ZOMBIE_GARGANTUAR,
	ZOMBIE_IMP,
	ZOMBIE_BOSS,
	ZOMBIE_PEA_HEAD,
	ZOMBIE_WALLNUT_HEAD,
	ZOMBIE_JALAPENO_HEAD,
	ZOMBIE_GATLING_HEAD,
	ZOMBIE_SQUASH_HEAD,
	ZOMBIE_TALLNUT_HEAD,
	ZOMBIE_REDEYE_GARGANTUAR,
	NUM_ZOMBIE_TYPES,
	ZOMBIE_CACHED_POLEVAULTER_WITH_POLE,
	NUM_CACHED_ZOMBIE_TYPES
};

enum ZombiePhase
{
	PHASE_ZOMBIE_NORMAL,
	PHASE_ZOMBIE_DYING,
	PHASE_ZOMBIE_BURNED,
	PHASE_ZOMBIE_MOWERED,
	PHASE_BUNGEE_DIVING,
	PHASE_BUNGEE_DIVING_SCREAMING,
	PHASE_BUNGEE_AT_BOTTOM,
	PHASE_BUNGEE_GRABBING,
	PHASE_BUNGEE_RISING,
	PHASE_BUNGEE_HIT_OUCHY,
	PHASE_BUNGEE_CUTSCENE,
	PHASE_POLEVAULTER_PRE_VAULT,
	PHASE_POLEVAULTER_IN_VAULT,
	PHASE_POLEVAULTER_POST_VAULT,
	PHASE_RISING_FROM_GRAVE,
	PHASE_JACK_IN_THE_BOX_RUNNING,
	PHASE_JACK_IN_THE_BOX_POPPING,
	PHASE_BOBSLED_SLIDING,
	PHASE_BOBSLED_BOARDING,
	PHASE_BOBSLED_CRASHING,
	PHASE_POGO_BOUNCING,
	PHASE_POGO_HIGH_BOUNCE_1,
	PHASE_POGO_HIGH_BOUNCE_2,
	PHASE_POGO_HIGH_BOUNCE_3,
	PHASE_POGO_HIGH_BOUNCE_4,
	PHASE_POGO_HIGH_BOUNCE_5,
	PHASE_POGO_HIGH_BOUNCE_6,
	PHASE_POGO_FORWARD_BOUNCE_2,
	PHASE_POGO_FORWARD_BOUNCE_7,
	PHASE_NEWSPAPER_READING,
	PHASE_NEWSPAPER_MADDENING,
	PHASE_NEWSPAPER_MAD,
	PHASE_DIGGER_TUNNELING,
	PHASE_DIGGER_RISING,
	PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE,
	PHASE_DIGGER_RISE_WITHOUT_AXE,
	PHASE_DIGGER_STUNNED,
	PHASE_DIGGER_WALKING,
	PHASE_DIGGER_WALKING_WITHOUT_AXE,
	PHASE_DIGGER_CUTSCENE,
	PHASE_DANCER_DANCING_IN,
	PHASE_DANCER_SNAPPING_FINGERS,
	PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT,
	PHASE_DANCER_SNAPPING_FINGERS_HOLD,
	PHASE_DANCER_DANCING_LEFT,
	PHASE_DANCER_WALK_TO_RAISE,
	PHASE_DANCER_RAISE_LEFT_1,
	PHASE_DANCER_RAISE_RIGHT_1,
	PHASE_DANCER_RAISE_LEFT_2,
	PHASE_DANCER_RAISE_RIGHT_2,
	PHASE_DANCER_RISING,
	PHASE_DOLPHIN_WALKING,
	PHASE_DOLPHIN_INTO_POOL,
	PHASE_DOLPHIN_RIDING,
	PHASE_DOLPHIN_IN_JUMP,
	PHASE_DOLPHIN_WALKING_IN_POOL,
	PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN,
	PHASE_SNORKEL_WALKING,
	PHASE_SNORKEL_INTO_POOL,
	PHASE_SNORKEL_WALKING_IN_POOL,
	PHASE_SNORKEL_UP_TO_EAT,
	PHASE_SNORKEL_EATING_IN_POOL,
	PHASE_SNORKEL_DOWN_FROM_EAT,
	PHASE_ZOMBIQUARIUM_ACCEL,
	PHASE_ZOMBIQUARIUM_DRIFT,
	PHASE_ZOMBIQUARIUM_BACK_AND_FORTH,
	PHASE_ZOMBIQUARIUM_BITE,
	PHASE_CATAPULT_LAUNCHING,
	PHASE_CATAPULT_RELOADING,
	PHASE_GARGANTUAR_THROWING,
	PHASE_GARGANTUAR_SMASHING,
	PHASE_IMP_GETTING_THROWN,
	PHASE_IMP_LANDING,
	PHASE_BALLOON_FLYING,
	PHASE_BALLOON_POPPING,
	PHASE_BALLOON_WALKING,
	PHASE_LADDER_CARRYING,
	PHASE_LADDER_PLACING,
	PHASE_BOSS_ENTER,
	PHASE_BOSS_IDLE,
	PHASE_BOSS_SPAWNING,
	PHASE_BOSS_STOMPING,
	PHASE_BOSS_BUNGEES_ENTER,
	PHASE_BOSS_BUNGEES_DROP,
	PHASE_BOSS_BUNGEES_LEAVE,
	PHASE_BOSS_DROP_RV,
	PHASE_BOSS_HEAD_ENTER,
	PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT,
	PHASE_BOSS_HEAD_IDLE_AFTER_SPIT,
	PHASE_BOSS_HEAD_SPIT,
	PHASE_BOSS_HEAD_LEAVE,
	PHASE_YETI_RUNNING,
	PHASE_SQUASH_PRE_LAUNCH,
	PHASE_SQUASH_RISING,
	PHASE_SQUASH_FALLING,
	PHASE_SQUASH_DONE_FALLING
};

enum ZombieHeight
{
	HEIGHT_ZOMBIE_NORMAL,
	HEIGHT_IN_TO_POOL,
	HEIGHT_OUT_OF_POOL,
	HEIGHT_DRAGGED_UNDER,
	HEIGHT_UP_TO_HIGH_GROUND,
	HEIGHT_DOWN_OFF_HIGH_GROUND,
	HEIGHT_UP_LADDER,
	HEIGHT_FALLING,
	HEIGHT_IN_TO_CHIMNEY,
	HEIGHT_GETTING_BUNGEE_DROPPED,
	HEIGHT_ZOMBIQUARIUM
};

enum ZombieAttackType
{
	ATTACKTYPE_CHEW,
	ATTACKTYPE_DRIVE_OVER,
	ATTACKTYPE_VAULT,
	ATTACKTYPE_LADDER
};

enum HelmType
{
	HELMTYPE_NONE,
	HELMTYPE_TRAFFIC_CONE,
	HELMTYPE_PAIL,
	HELMTYPE_FOOTBALL,
	HELMTYPE_DIGGER,
	HELMTYPE_REDEYES,
	HELMTYPE_HEADBAND,
	HELMTYPE_BOBSLED,
	HELMTYPE_WALLNUT,
	HELMTYPE_TALLNUT
};

enum ShieldType
{
	SHIELDTYPE_NONE,
	SHIELDTYPE_DOOR,
	SHIELDTYPE_NEWSPAPER,
	SHIELDTYPE_LADDER
};

enum ZombieParts
{
	PART_BODY,
	PART_HEAD,
	PART_HEAD_EATING,
	PART_TONGUE,
	PART_ARM,
	PART_HAIR,
	PART_HEAD_YUCKY,
	PART_ARM_PICKAXE,
	PART_ARM_POLEVAULT,
	PART_ARM_LEASH,
	PART_ARM_FLAG,
	PART_POGO,
	PART_DIGGER
};

enum BossPart
{
	BOSS_PART_BACK_LEG,
	BOSS_PART_FRONT_LEG,
	BOSS_PART_MAIN,
	BOSS_PART_BACK_ARM,
	BOSS_PART_FIREBALL
};

enum ZombieRenderLayerOffset
{
	ZOMBIE_LAYER_OFFSET_BOBSLED_4,
	ZOMBIE_LAYER_OFFSET_BOBSLED_3,
	ZOMBIE_LAYER_OFFSET_BOBSLED_2,
	ZOMBIE_LAYER_OFFSET_BOBSLED_1,
	ZOMBIE_LAYER_OFFSET_NORMAL,
	ZOMBIE_LAYER_OFFSET_DOG_WALKER,
	ZOMBIE_LAYER_OFFSET_DOG,
	ZOMBIE_LAYER_OFFSET_DIGGER,
	ZOMBIE_LAYER_OFFSET_ZAMBONI
};

enum ZombieID
{
	ZOMBIEID_NULL
};

enum DamageFlags
{
	DAMAGE_BYPASSES_SHIELD,
	DAMAGE_HITS_SHIELD_AND_BODY,
	DAMAGE_FREEZE,
	DAMAGE_DOESNT_CAUSE_FLASH,
	DAMAGE_DOESNT_LEAVE_BODY,
	DAMAGE_SPIKE
};

class Zombie : public GameObject
{
public:
	enum
	{
		ZOMBIE_WAVE_DEBUG			= -1,
		ZOMBIE_WAVE_CUTSCENE		= -2,
		ZOMBIE_WAVE_UI				= -3,
		ZOMBIE_WAVE_WINNER			= -4
	};

public:
	ZombieType						mZombieType;								//+0x24
	ZombiePhase						mZombiePhase;								//+0x28
	float							mPosX;										//+0x2C
	float							mPosY;										//+0x30
	float							mVelX;										//+0x34
	int								mAnimCounter;								//+0x38
	int								mGroanCounter;								//+0x3C
	int								mAnimTicksPerFrame;							//+0x40
	int								mAnimFrames;								//+0x44
	int								mFrame;										//+0x48
	int								mPrevFrame;									//+0x4C
	bool							mVariant;									//+0x50
	bool							mIsEating;									//+0x51
	int								mJustGotShotCounter;						//+0x54
	int								mShieldJustGotShotCounter;					//+0x58
	int								mShieldRecoilCounter;						//+0x5C
	int								mZombieAge;									//+0x60
	ZombieHeight					mZombieHeight;								//+0x64
	int								mPhaseCounter;								//+0x68
	int								mFromWave;									//+0x6C
	bool							mDroppedLoot;								//+0x70
	int								mZombieFade;								//+0x74
	bool							mFlatTires;									//+0x78
	int								mUseLadderCol;								//+0x7C
	int								mTargetCol;									//+0x80
	float							mAltitude;									//+0x84
	bool							mHitUmbrella;								//+0x88
	Sexy::Rect						mZombieRect;								//+0x8C
	Sexy::Rect						mZombieAttackRect;							//+0x9C
	int								mChilledCounter;							//+0xAC
	int								mButteredCounter;							//+0xB0
	int								mIceTrapCounter;							//+0xB4
	bool							mMindControlled;							//+0xB8
	bool							mBlowingAway;								//+0xB9
	bool							mHasHead;									//+0xBA
	bool							mHasArm;									//+0xBB
	bool							mHasObject;									//+0xBC
	bool							mInPool;									//+0xBD
	bool							mOnHighGround;								//+0xBE
	bool							mYuckyFace;									//+0xBF
	int								mYuckyFaceCounter;							//+0xC0
	HelmType						mHelmType;									//+0xC4
	int								mBodyHealth;								//+0xC8
	int								mBodyMaxHealth;								//+0xCC
	int								mHelmHealth;								//+0xD0
	int								mHelmMaxHealth;								//+0xD4
	ShieldType						mShieldType;								//+0xD8
	int								mShieldHealth;								//+0xDC
	int								mShieldMaxHealth;							//+0xE0
	int								mFlyingHealth;								//+0xE4
	int								mFlyingMaxHealth;							//+0xE8
	bool							mDead;										//+0xEC
	ZombieID						mRelatedZombieID;							//+0xF0
	ZombieID						mFollowerZombieID[MAX_ZOMBIE_FOLLOWERS];	//+0xF4
	bool							mPlayingSong;								//+0x104
	int								mParticleOffsetX;							//+0x108
	int								mParticleOffsetY;							//+0x10C
	AttachmentID					mAttachmentID;								//+0x110
	int								mSummonCounter;								//+0x114
	ReanimationID					mBodyReanimID;								//+0x118
	float							mScaleZombie;								//+0x11C
	float							mVelZ;										//+0x120
	float							mOriginalAnimRate;							//+0x124
	PlantID							mTargetPlantID;								//+0x128
	int								mBossMode;									//+0x12C
	int								mTargetRow;									//+0x130
	int								mBossBungeeCounter;							//+0x134
	int								mBossStompCounter;							//+0x138
	int								mBossHeadCounter;							//+0x13C
	ReanimationID					mBossFireBallReanimID;						//+0x140
	ReanimationID					mSpecialHeadReanimID;						//+0x144
	int								mFireballRow;								//+0x148
	bool							mIsFireBall;								//+0x14C
	ReanimationID					mMoweredReanimID;							//+0x150
	int								mLastPortalX;								//+0x154

public:
	Zombie();
	~Zombie();

	void							ZombieInitialize(int theRow, ZombieType theType, bool theVariant, Zombie* theParentZombie, int theFromWave);
	void							Animate();
	void							CheckIfPreyCaught();
	void							EatZombie(Zombie* theZombie);
	void							EatPlant(Plant* thePlant);
	void							Update();
	void							DieNoLoot();
	void							DieWithLoot();
	void							Draw(Sexy::Graphics* g);
	void							DrawZombie(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	void							DrawZombieWithParts(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	void							DrawZombiePart(Sexy::Graphics* g, Sexy::Image* theImage, int theFrame, int theRow, const ZombieDrawPosition& theDrawPos);
	void							DrawBungeeCord(Sexy::Graphics* g, int theOffsetX, int theOffsetY);
	void							TakeDamage(int theDamage, unsigned int theDamageFlags);
	void							SetRow(int theRow);
	float							GetPosYBasedOnRow(int theRow);
	void							ApplyChill(bool theIsIceTrap);
	void							UpdateZombieBungee();
	void							BungeeLanding();
	bool							EffectedByDamage(unsigned int theDamageRangeFlags);
	void							PickRandomSpeed();
	void							UpdateZombiePolevaulter();
	void							UpdateZombieDolphinRider();
	void							PickBungeeZombieTarget(int theColumn);
	int								CountBungeesTargetingSunFlowers();
	Plant*							FindPlantTarget(ZombieAttackType theAttackType);
	void							CheckSquish(ZombieAttackType theAttackType);
	void							RiseFromGrave(int theCol, int theRow);
	void							UpdateZombieRiseFromGrave();
	void							UpdateDamageStates(unsigned int theDamageFlags);
	void							UpdateZombiePool();
	void							CheckForPool();
	void							GetDrawPos(ZombieDrawPosition& theDrawPos);
	void							UpdateZombieHighGround();
	void							CheckForHighGround();
	bool							IsOnHighGround();
	void							DropLoot();
	bool							TrySpawnLevelAward();
	void							StartZombieSound();
	void							StopZombieSound();
	void							UpdateZombieJackInTheBox();
	void							DrawZombieHead(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos, int theFrame);
	void							UpdateZombiePosition();
	Sexy::Rect						GetZombieRect();
	Sexy::Rect						GetZombieAttackRect();
	void							UpdateZombieWalking();
	void							UpdateZombieBobsled();
	void							BobsledCrash();
	Plant*							IsStandingOnSpikeweed();
	void							CheckForZombieStep();
	void							CountExpectedMowers() { ; }
	void							OverrideParticleColor(TodParticleSystem* aParticle);
	void							OverrideParticleScale(TodParticleSystem* aParticle);
	void							PoolSplash(bool theInToPoolSound);
	void							UpdateZombieFlyer();
	void							UpdateZombiePogo();
	void							UpdateZombieNewspaper();
	void							LandFlyer(unsigned int theDamageFlags);
	void							UpdateZombieDigger();
	bool							IsWalkingBackwards();
	TodParticleSystem*				AddAttachedParticle(int thePosX, int thePosY, ParticleEffect theEffect);
	void							PogoBreak(unsigned int theDamageFlags);
	void							UpdateZombieFalling();
	void							UpdateZombieDancer();
	ZombieID						SummonBackupDancer(int theRow, int thePosX);
	void							SummonBackupDancers();
	int								GetDancerFrame();
	void							BungeeStealTarget();
	void							BungeeLiftTarget();
	void							UpdateYuckyFace();
	void							DrawIceTrap(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos, bool theFront);
	void							HitIceTrap();
	int								GetHelmDamageIndex();
	int								GetShieldDamageIndex();
	void							DrawReanim(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos, int theBaseRenderGroup);
	void							UpdatePlaying();
	bool							NeedsMoreBackupDancers();
	void							ConvertToNormalZombie();
	void							UpdateDancerWalking() { ; }
	void							StartEating();
	void							StopEating();
	void							UpdateAnimSpeed();
	void							ReanimShowPrefix(const char* theTrackPrefix, int theRenderGroup);
	void							PlayDeathAnim(unsigned int theDamageFlags);
	void							UpdateDeath();
	void							DrawShadow(Sexy::Graphics* g);
	bool							HasShadow();
	Reanimation*					LoadReanim(ReanimationType theReanimationType);
	int								TakeFlyingDamage(int theDamage, unsigned int theDamageFlags);
	int								TakeShieldDamage(int theDamage, unsigned int theDamageFlags);
	int								TakeHelmDamage(int theDamage, unsigned int theDamageFlags);
	void							TakeBodyDamage(int theDamage, unsigned int theDamageFlags);
	void							AttachShield();
	void							DetachShield();
	void							UpdateReanim();
	void							GetTrackPosition(const char* theTrackName, float& thePosX, float& thePosY);
	void							LoadPlainZombieReanim();
	void							ShowDoorArms(bool theShow);
	void							ReanimShowTrack(const char* theTrackName, int theRenderGroup);
	void							PlayZombieAppearSound();
	void							StartMindControlled();
	bool							IsFlying();
	void							DropHead(unsigned int theDamageFlags);
	bool							CanTargetPlant(Plant* thePlant, ZombieAttackType theAttackType);
	void							UpdateZombieCatapult();
	Plant*							FindCatapultTarget();
	void							ZombieCatapultFire(Plant* thePlant);
	void							UpdateClimbingLadder();
	void							UpdateZombieGargantuar();
	int								GetBodyDamageIndex();
	void							ApplyBurn();
	void							UpdateBurn();
	bool							ZombieNotWalking();
	Zombie*							FindZombieTarget();
	void							PlayZombieReanim(const char* theTrackName, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate);
	void							UpdateZombieBackupDancer();
	ZombiePhase						GetDancerPhase();
	bool							IsMovingAtChilledSpeed();
	void							StartWalkAnim(int theBlendTime);
	Reanimation*					AddAttachedReanim(int thePosX, int thePosY, ReanimationType theReanimType);
	void							DragUnder();
	static void						SetupDoorArms(Reanimation* aReanim, bool theShow);
	static void						SetupReanimLayers(Reanimation* aReanim, ZombieType theZombieType);
	bool							IsOnBoard();
	void							DrawButter(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	bool							IsImmobilizied();
	void							ApplyButter();
	float							ZombieTargetLeadX(float theTime);
	void							UpdateZombieImp();
	void							SquishAllInSquare(int theX, int theY, ZombieAttackType theAttackType);
	void							RemoveIceTrap();
	bool							IsBouncingPogo();
	int								GetBobsledPosition();
	void							DrawBobsledReanim(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos, bool theBeforeZombie);
	void							BobsledDie();
	void							BobsledBurn();
	bool							IsBobsledTeamWithSled();
	bool							CanBeFrozen();
	bool							CanBeChilled();
	void							UpdateZombieSnorkel();
	void							ReanimIgnoreClipRect(const char* theTrackName, bool theIgnoreClipRect);
	void							SetAnimRate(float theAnimRate);
	void							ApplyAnimRate(float theAnimRate);
	bool							IsDeadOrDying();
	void							DrawDancerReanim(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	void							DrawBungeeReanim(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	void							DrawBungeeTarget(Sexy::Graphics* g);
	void							BungeeDie();
	void							ZamboniDeath(unsigned int theDamageFlags);
	void							CatapultDeath(unsigned int theDamageFlags);
	bool							SetupDrawZombieWon(Sexy::Graphics* g);
	void							WalkIntoHouse();
	void							UpdateZamboni();
	void							UpdateZombieChimney();
	void							UpdateLadder();
	void							DropArm(unsigned int theDamageFlags);
	bool							CanLoseBodyParts();
	void							DropHelm(unsigned int theDamageFlags);
	void							DropShield(unsigned int theDamageFlags);
	void							ReanimReenableClipping();
	void							UpdateBoss();
	void							BossPlayIdle();
	void							BossRVLanding();
	void							BossStompContact();
	bool							BossAreBungeesDone();
	void							BossBungeeSpawn();
	void							BossSpawnAttack();
	void							BossBungeeAttack();
	void							BossRVAttack();
	void							BossSpawnContact();
	void							BossBungeeLeave();
	void							BossStompAttack();
	bool							BossCanStompRow(int theRow);
	void							BossDie();
	void							BossHeadAttack();
	void							BossHeadSpitContact();
	void							BossHeadSpit();
	void							UpdateBossFireball();
	void							BossDestroyFireball();
	void							BossDestroyIceballInRow(int theRow);
	void							DiggerLoseAxe();
	void							BungeeDropZombie(Zombie* theDroppedZombie, int theGridX, int theGridY);
	void							ShowYuckyFace(bool theShow);
	void							AnimateChewSound();
	void							AnimateChewEffect();
	void							UpdateActions();
	void							CheckForBoardEdge();
	void							UpdateYeti();
	void							DrawBossPart(Sexy::Graphics* g, BossPart theBossPart);
	void							BossSetupReanim();
	void							MowDown();
	void							UpdateMowered();
	void							DropFlag();
	void							DropPole();
	void							DrawBossBackArm(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	static void						PreloadZombieResources(ZombieType theZombieType);
	void							BossStartDeath();
	void							RemoveColdEffects();
	void							BossHeadSpitEffect();
	void							DrawBossFireBall(Sexy::Graphics* g, const ZombieDrawPosition& theDrawPos);
	void							UpdateZombiePeaHead();
	void							UpdateZombieJalapenoHead();
	void							ApplyBossSmokeParticles(bool theEnable);
	void							UpdateZombiquarium();
	bool							ZombiquariumFindClosestBrain();
	void							UpdateZombieGatlingHead();
	void							UpdateZombieSquashHead();
	bool							IsTanglekelpTarget();
	bool							HasYuckyFaceImage();
	bool							IsTangleKelpTarget();
	bool							IsFireResistant();
	void							EnableMustache(bool theEnableMustache);
	void							EnableFuture(bool theEnableFuture);
	void							EnableDance(bool theEnableDance);
	void							BungeeDropPlant();
	void							RemoveButter();
	void							BalloonPropellerHatSpin(bool theSpinning);
	void							DoDaisies();
	static bool						ZombieTypeCanGoOnHighGround(ZombieType theZombieType);
	static bool						ZombieTypeCanGoInPool(ZombieType theZombieType);
	void							SetupWaterTrack(const char* theTrackName);
	void							BurnRow(int theRow);
	void							SetupReanimForLostHead();
	void							SetupReanimForLostArm(unsigned int theDamageFlags);
	bool							IsSquashTarget(Plant* theExcept);
	static bool						IsZombotany(ZombieType theZombieType);
};

class ZombieDefinition
{
public:
	ZombieType						mZombieType;
	ReanimationType					mReanimationType;
	int								mZombieValue;
	int								mStartingLevel;
	int								mFirstAllowedWave;
	int								mPickWeight;
	const SexyChar*					mZombieName;
};
extern ZombieDefinition gZombieDefs[NUM_ZOMBIE_TYPES];  //0x69DA80

ZombieDefinition&					GetZombieDefinition(ZombieType theZombieType);

#endif