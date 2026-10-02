#ifndef __TODPARTICLE_H__
#define __TODPARTICLE_H__

#include "TodList.h"
#include "DataArray.h"
#include "Definition.h"
#include "../SexyAppFramework/SexyVector.h"

namespace Sexy
{
	class Image;
	class Graphics;
};

#define MAX_PARTICLES_SIZE 900
#define MAX_PARTICLE_FIELDS 4

// ######################################################################################################################################################
// ############################################################# 以下为粒子系统定义相关内容 #############################################################
// ######################################################################################################################################################

enum ParticleFlags
{
    PARTICLE_RANDOM_LAUNCH_SPIN,        // 随机发射旋转，指定粒子在发射时使用 [0, 2π] 内随机的初始旋转角度
    PARTICLE_ALIGN_LAUNCH_SPIN,         // 对齐发射角度，指定粒子在发射时的初始旋转角度与发射方向一致（优先级低于随机发射旋转）
    PARTICLE_ALIGN_TO_PIXELS,           // 对齐至像素，指定粒子渲染时的坐标四舍五入地对齐至整数像素值
    PARTICLE_SYSTEM_LOOPS,              // 系统循环，指定发射器在生命周期结束后立即回到周期起始的时刻
    PARTICLE_PARTICLE_LOOPS,            // 粒子循环，指定粒子在生命周期结束后立即回到周期起始的时刻
    PARTICLE_PARTICLES_DONT_FOLLOW,     // 粒子不跟随，指定当粒子发射器的位置移动时，已发射的粒子不随之移动
    PARTICLE_RANDOM_START_TIME,         // 随机初始时刻，指定粒子初始时的存在时间为 0 至最大持续时间之间的随机值
    PARTICLE_DIE_IF_OVERLOADED,         // 过载限制，指定在粒子数量过多时，该发射器所属的粒子系统无法被创建
    PARTICLE_ADDITIVE,                  // 叠加模式，指定粒子渲染时固定使用叠加（Additive）模式
    PARTICLE_FULLSCREEN,                // 全屏模式，指定粒子在渲染时改为填充一个屏幕大小的矩形
    PARTICLE_SOFTWARE_ONLY,             // 仅软件渲染，指定粒子仅在未开启 3D 加速时可被渲染
    PARTICLE_HARDWARE_ONLY              // 仅硬件渲染，指定粒子仅在已开启 3D 加速时可被渲染
};

enum ParticleFieldType
{
    FIELD_INVALID,
    FIELD_FRICTION,                     // 摩擦力场：该场内粒子的速度按一定比例不断衰减
    FIELD_ACCELERATION,                 // 加速度场：该场内粒子以一定加速度做加速运动
    FIELD_ATTRACTOR,                    // 弹性力场：该场内粒子加速度的大小和方向均受到粒子与发射器之间距离大小的影响
    FIELD_MAX_VELOCITY,                 // 限速场：该场内粒子速度的大小不能超过一定的上限值
    FIELD_VELOCITY,                     // 匀速场：该场内粒子速度的大小总是为一给定值
    FIELD_POSITION,                     // 定位场：该场内粒子的位置总是为一给定值
    FIELD_SYSTEM_POSITION,              // 系统定位场：仅发射器可用，该场内发射器的位置总是为一给定值
    FIELD_GROUND_CONSTRAINT,            // 地面限制：粒子的纵向位置不能低于地面，且接触地面时会触发弹起效果
    FIELD_SHAKE,                        // 震动：粒子的坐标会有随机 -1.0 到 +1.0 的偏移，每帧独立计算
    FIELD_CIRCLE,                       // 引力场：该场内粒子围绕发射器中心做圆周运动（考虑误差，实为螺线运动）
    FIELD_AWAY,                         // 斥力场：该场内粒子沿径向不断远离发射器中心
    PARTICLE_FIELD_COUNT
};  // 粒子场相关内容详见 TodParticleEmitter::UpdateParticleField() 函数（系统定位场为 TodParticleEmitter::UpdateSystemField() 函数）

// ################################################################################
// ▲ 以下粒子系统定义相关的各类型的实例与 XML 中的格式的对应关系示例简图
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// <Emitter>                                                         —
//                       ↓←Node→↓                                ↑
//     <SpawnRate>[.5 2] [2.5 4.5],40 [5 10]</SpawnRate>    TodEmitterDefinition
//     ↑←      FloatParameterTrack::mNodes        →↑             ↓
// </Emitter>                                                        —
// ################################################################################
// 【!】XML 中没有给出的定义项，将会在读取结束后被赋予该项的默认值

// ====================================================================================================
// ★ 【粒子场】
// ----------------------------------------------------------------------------------------------------
// 发射器中粒子运动的物理环境。多个粒子场可以叠加存在，最多叠加 4 个。
// ====================================================================================================
class ParticleField
{
public:
    ParticleFieldType           mFieldType;                     // 场的类型，决定了场对粒子运动的影响方式
    FloatParameterTrack         mX;                             // 场在水平方向（横向）上对粒子的影响
    FloatParameterTrack         mY;                             // 场在竖直方向（纵向）上对粒子的影响
};

// ====================================================================================================
// ★ 【发射器定义】
// ----------------------------------------------------------------------------------------------------
// 粒子发射器的定义数据描述了其各种行为的参数的变化规律和范围。
// ====================================================================================================

enum EmitterType
{
    EMITTER_CIRCLE,
    EMITTER_BOX,
    EMITTER_BOX_PATH,
    EMITTER_CIRCLE_PATH,
    EMITTER_CIRCLE_EVEN_SPACING
};

class TodEmitterDefinition
{
public:
    Sexy::Image*                mImage;
    int                         mImageCol;
    int                         mImageRow;
    int                         mImageFrames;
    int                         mAnimated;
    int                         mParticleFlags;
    EmitterType                 mEmitterType;
    const char*                 mName;
    const char*                 mOnDuration;
    FloatParameterTrack         mSystemDuration;
    FloatParameterTrack         mCrossFadeDuration;
    FloatParameterTrack         mSpawnRate;
    FloatParameterTrack         mSpawnMinActive;
    FloatParameterTrack         mSpawnMaxActive;
    FloatParameterTrack         mSpawnMaxLaunched;
    FloatParameterTrack         mEmitterRadius;
    FloatParameterTrack         mEmitterOffsetX;
    FloatParameterTrack         mEmitterOffsetY;
    FloatParameterTrack         mEmitterBoxX;
    FloatParameterTrack         mEmitterBoxY;
    FloatParameterTrack         mEmitterSkewX;
    FloatParameterTrack         mEmitterSkewY;
    FloatParameterTrack         mEmitterPath;
    FloatParameterTrack         mParticleDuration;
    FloatParameterTrack         mLaunchSpeed;
    FloatParameterTrack         mLaunchAngle;
    FloatParameterTrack         mSystemRed;
    FloatParameterTrack         mSystemGreen;
    FloatParameterTrack         mSystemBlue;
    FloatParameterTrack         mSystemAlpha;
    FloatParameterTrack         mSystemBrightness;
    ParticleField*              mParticleFields;
    int                         mParticleFieldCount;
    ParticleField*              mSystemFields;
    int                         mSystemFieldCount;
    FloatParameterTrack         mParticleRed;
    FloatParameterTrack         mParticleGreen;
    FloatParameterTrack         mParticleBlue;
    FloatParameterTrack         mParticleAlpha;
    FloatParameterTrack         mParticleBrightness;
    FloatParameterTrack         mParticleSpinAngle;
    FloatParameterTrack         mParticleSpinSpeed;
    FloatParameterTrack         mParticleScale;
    FloatParameterTrack         mParticleStretch;
    FloatParameterTrack         mCollisionReflect;
    FloatParameterTrack         mCollisionSpin;
    FloatParameterTrack         mClipTop;
    FloatParameterTrack         mClipBottom;
    FloatParameterTrack         mClipLeft;
    FloatParameterTrack         mClipRight;
    FloatParameterTrack         mAnimationRate;
};

// ====================================================================================================
// ★ 【粒子系统定义】
// ----------------------------------------------------------------------------------------------------
// 粒子系统的定义数据，是粒子系统中各个粒子发射器的定义数据的集合。
// ====================================================================================================
class TodParticleDefinition
{
public:
    TodEmitterDefinition*       mEmitterDefs;
    int                         mEmitterDefCount;
};

extern int gParticleDefCount;                       // [0x6A9F08]
extern TodParticleDefinition* gParticleDefArray;    // [0x6A9F0C]。于 LawnApp::LoadingThreadProc() 函数中读取并赋值

void* __cdecl                   TodParticleDefinitionConstructor(void* thePointer); //0x5155A0
void* __cdecl                   TodEmitterDefinitionConstructor(void* thePointer);  //0x5155C0
void* __cdecl                   ParticleFieldConstructor(void* thePointer);         //0x515620

extern DefMap gParticleFieldDefMap;  //0x69E338
extern DefMap gEmitterDefMap;  //0x69E344
extern DefMap gParticleDefMap;  //0x69E690

// ====================================================================================================
// ★ 粒子系统类型
// ====================================================================================================
enum ParticleEffect
{
	PARTICLE_NONE = -1,
	PARTICLE_MELONSPLASH,
	PARTICLE_WINTERMELON,
	PARTICLE_FUMECLOUD,
	PARTICLE_POPCORNSPLASH,
	PARTICLE_POWIE,
	PARTICLE_JACKEXPLODE,
	PARTICLE_ZOMBIE_HEAD,
	PARTICLE_ZOMBIE_ARM,
	PARTICLE_ZOMBIE_TRAFFIC_CONE,
	PARTICLE_ZOMBIE_PAIL,
	PARTICLE_ZOMBIE_HELMET,
	PARTICLE_ZOMBIE_FLAG,
	PARTICLE_ZOMBIE_DOOR,
	PARTICLE_ZOMBIE_NEWSPAPER,
	PARTICLE_ZOMBIE_HEADLIGHT,
	PARTICLE_POW,
	PARTICLE_ZOMBIE_POGO,
	PARTICLE_ZOMBIE_NEWSPAPER_HEAD,
	PARTICLE_ZOMBIE_BALLOON_HEAD,
	PARTICLE_SOD_ROLL,
	PARTICLE_GRAVE_STONE_RISE,
	PARTICLE_PLANTING,
	PARTICLE_PLANTING_POOL,
	PARTICLE_ZOMBIE_RISE,
	PARTICLE_GRAVE_BUSTER,
	PARTICLE_GRAVE_BUSTER_DIE,
	PARTICLE_POOL_SPLASH,
	PARTICLE_ICE_SPARKLE,
	PARTICLE_SEED_PACKET,
	PARTICLE_TALL_NUT_BLOCK,
	PARTICLE_DOOM,
	PARTICLE_DIGGER_RISE,
	PARTICLE_DIGGER_TUNNEL,
	PARTICLE_DANCER_RISE,
	PARTICLE_POOL_SPARKLY,
	PARTICLE_WALLNUT_EAT_SMALL,
	PARTICLE_WALLNUT_EAT_LARGE,
	PARTICLE_PEA_SPLAT,
	PARTICLE_BUTTER_SPLAT,
	PARTICLE_CABBAGE_SPLAT,
	PARTICLE_PUFF_SPLAT,
	PARTICLE_STAR_SPLAT,
	PARTICLE_ICE_TRAP,
	PARTICLE_SNOWPEA_SPLAT,
	PARTICLE_SNOWPEA_PUFF,
	PARTICLE_SNOWPEA_TRAIL,
	PARTICLE_LANTERN_SHINE,
	PARTICLE_SEED_PACKET_PICKUP,
	PARTICLE_POTATO_MINE,
	PARTICLE_POTATO_MINE_RISE,
	PARTICLE_PUFFSHROOM_TRAIL,
	PARTICLE_PUFFSHROOM_MUZZLE,
	PARTICLE_SEED_PACKET_FLASH,
	PARTICLE_WHACK_A_ZOMBIE_RISE,
	PARTICLE_ZOMBIE_LADDER,
	PARTICLE_UMBRELLA_REFLECT,
	PARTICLE_SEED_PACKET_PICK,
	PARTICLE_ICE_TRAP_ZOMBIE,
	PARTICLE_ICE_TRAP_RELEASE,
	PARTICLE_ZAMBONI_SMOKE,
	PARTICLE_GLOOMCLOUD,
	PARTICLE_ZOMBIE_POGO_HEAD,
	PARTICLE_ZAMBONI_TIRE,
	PARTICLE_ZAMBONI_EXPLOSION,
	PARTICLE_ZAMBONI_EXPLOSION2,
	PARTICLE_CATAPULT_EXPLOSION,
	PARTICLE_MOWER_CLOUD,
	PARTICLE_BOSS_ICE_BALL,
	PARTICLE_BLASTMARK,
	PARTICLE_COIN_PICKUP_ARROW,
	PARTICLE_PRESENT_PICKUP,
	PARTICLE_IMITATER_MORPH,
	PARTICLE_MOWERED_ZOMBIE_HEAD,
	PARTICLE_MOWERED_ZOMBIE_ARM,
	PARTICLE_ZOMBIE_HEAD_POOL,
	PARTICLE_ZOMBIE_BOSS_FIREBALL,
	PARTICLE_FIREBALL_DEATH,
	PARTICLE_ICEBALL_DEATH,
	PARTICLE_ICEBALL_TRAIL,
	PARTICLE_FIREBALL_TRAIL,
	PARTICLE_BOSS_EXPLOSION,
	PARTICLE_SCREEN_FLASH,
	PARTICLE_TROPHY_SPARKLE,
	PARTICLE_PORTAL_CIRCLE,
	PARTICLE_PORTAL_SQUARE,
	PARTICLE_POTTED_PLANT_GLOW,
	PARTICLE_POTTED_WATER_PLANT_GLOW,
	PARTICLE_POTTED_ZEN_GLOW,
	PARTICLE_MIND_CONTROL,
	PARTICLE_VASE_SHATTER,
	PARTICLE_VASE_SHATTER_LEAF,
	PARTICLE_VASE_SHATTER_ZOMBIE,
	PARTICLE_AWARD_PICKUP_ARROW,
	PARTICLE_ZOMBIE_SEAWEED,
	PARTICLE_ZOMBIE_MUSTACHE,
	PARTICLE_ZOMBIE_SUNGLASS,
	PARTICLE_ZOMBIE_PINATA,
	PARTICLE_DUST_SQUASH,
	PARTICLE_DUST_FOOT,
	PARTICLE_ZOMBIE_DAISIES,
	PARTICLE_CREDIT_STROBE,
	PARTICLE_CREDITS_RAYSWIPE,
	PARTICLE_CREDITS_ZOMBIEHEADWIPE,
	PARTICLE_STARBURST,
	PARTICLE_CREDITS_FOG,
	PARTICLE_PERSENT_PICK_UP_ARROW,
	NUM_PARTICLES
};

// ====================================================================================================
// ★ 【粒子参数】
// ----------------------------------------------------------------------------------------------------
// 用于描述一种粒子系统类型与该粒子系统的数据文件的文件名之间的对应关系。
// ====================================================================================================
class ParticleParams
{
public:
    ParticleEffect              mParticleEffect;
    const char*                 mParticleFileName;
};
extern int gParticleParamArraySize;                 // [0x6A9F10]
extern ParticleParams* gParticleParamArray;         // [0x6A9F14]

bool                            TodParticleLoadADef(TodParticleDefinition* theParticleDef, const char* theParticleFileName);
void                            TodParticleLoadDefinitions(ParticleParams* theParticleParamArray, int theParticleParamArraySize);
void                            TodParticleFreeDefinitions();

// ######################################################################################################################################################
// ############################################################ 以下正式开始粒子系统相关声明 ############################################################
// ######################################################################################################################################################

enum ParticleSystemTracks
{
	TRACK_SPAWN_RATE,
	TRACK_SPAWN_MIN_ACTIVE,
	TRACK_SPAWN_MAX_ACTIVE,
	TRACK_SPAWN_MAX_LAUNCHED,
	TRACK_EMITTER_PATH,
	TRACK_SYSTEM_RED,
	TRACK_SYSTEM_GREEN,
	TRACK_SYSTEM_BLUE,
	TRACK_SYSTEM_ALPHA,
	TRACK_SYSTEM_BRIGHTNESS,
	NUM_SYSTEM_TRACKS
};

enum ParticleTracks
{
	TRACK_PARTICLE_RED,
	TRACK_PARTICLE_GREEN,
	TRACK_PARTICLE_BLUE,
	TRACK_PARTICLE_ALPHA,
	TRACK_PARTICLE_BRIGHTNESS,
	TRACK_PARTICLE_SPIN_SPEED,
	TRACK_PARTICLE_SPIN_ANGLE,
	TRACK_PARTICLE_SCALE,
	TRACK_PARTICLE_STRETCH,
	TRACK_PARTICLE_COLLISION_REFLECT,
	TRACK_PARTICLE_COLLISION_SPIN,
	TRACK_PARTICLE_CLIP_TOP,
	TRACK_PARTICLE_CLIP_BOTTOM,
	TRACK_PARTICLE_CLIP_LEFT,
	TRACK_PARTICLE_CLIP_RIGHT,
	TRACK_PARTICLE_ANIMATION_RATE,
	NUM_PARTICLE_TRACKS
};

class TodParticleSystem;
class TodParticleEmitter;
class TodParticle;

enum ParticleID         { PARTICLEID_NULL };
enum ParticleEmitterID  { PARTICLEEMITTERID_NULL };
enum ParticleSystemID   { PARTICLESYSTEMID_NULL };

class TodParticleHolder
{
public:
	DataArray<TodParticleSystem>	mParticleSystems;
	DataArray<TodParticleEmitter>	mEmitters;
	DataArray<TodParticle>			mParticles;
	TodAllocator					mParticleListNodeAllocator;
	TodAllocator					mEmitterListNodeAllocator;

public:
    ~TodParticleHolder();

    void							InitializeHolder();
    void							DisposeHolder();
    TodParticleSystem*				AllocParticleSystemFromDef(float theX, float theY, int theRenderOrder, TodParticleDefinition* theDefinition, ParticleEffect theParticleEffect);
    TodParticleSystem*				AllocParticleSystem(float theX, float theY, int theRenderOrder, ParticleEffect theParticleEffect);
    bool					        IsOverLoaded();
};

class ParticleRenderParams
{
public:
	bool							mRedIsSet;
	bool							mGreenIsSet;
	bool							mBlueIsSet;
	bool							mAlphaIsSet;
	bool							mParticleScaleIsSet;
	bool							mParticleStretchIsSet;
	bool							mSpinPositionIsSet;
	bool							mPositionIsSet;
	float							mRed;
	float							mGreen;
	float							mBlue;
	float							mAlpha;
	float							mParticleScale;
	float							mParticleStretch;
	float							mSpinPosition;
	float							mPosX;
	float							mPosY;
};

class TodParticle
{
public:
	TodParticleEmitter*				mParticleEmitter;
	int								mParticleDuration;
	int								mParticleAge;
	float							mParticleTimeValue;
	float							mParticleLastTimeValue;
	float							mAnimationTimeValue;
	Sexy::SexyVector2				mVelocity;
	Sexy::SexyVector2				mPosition;
	int								mImageFrame;
	float							mSpinPosition;
	float							mSpinVelocity;
	ParticleID						mCrossFadeParticleID;
	int								mCrossFadeDuration;
	float							mParticleInterp[ParticleTracks::NUM_PARTICLE_TRACKS];
	float							mParticleFieldInterp[MAX_PARTICLE_FIELDS][2];
};

class TodTriangleGroup;
class TodParticleEmitter
{
public:
	TodEmitterDefinition*			mEmitterDef;													//+0x0
	TodParticleSystem*				mParticleSystem;												//+0x4
	TodList<ParticleID>				mParticleList;													//+0x8
	float							mSpawnAccum;													//+0x18
	Sexy::SexyVector2				mSystemCenter;													//+0x1C
	int								mParticlesSpawned;												//+0x24
	int								mSystemAge;														//+0x28
	int								mSystemDuration;												//+0x2C
	float							mSystemTimeValue;												//+0x30
	float							mSystemLastTimeValue;											//+0x34
	bool							mDead;															//+0x38
	Sexy::Color						mColorOverride;													//+0x3C
	bool							mExtraAdditiveDrawOverride;										//+0x4C
	float							mScaleOverride;													//+0x50
	Sexy::Image*					mImageOverride;													//+0x54
	ParticleEmitterID				mCrossFadeEmitterID;											//+0x58
	int								mEmitterCrossFadeCountDown;										//+0x5C
	int								mFrameOverride;													//+0x60
	float							mTrackInterp[ParticleSystemTracks::NUM_SYSTEM_TRACKS];			//+0x64
	float							mSystemFieldInterp[MAX_PARTICLE_FIELDS][2];						//+0x8C

public:
	void							TodEmitterInitialize(float theX, float theY, TodParticleSystem* theSystem, TodEmitterDefinition* theEmitterDef);
	void							Update();
	void							Draw(Sexy::Graphics* g);
	void							SystemMove(float theX, float theY);
	static bool						GetRenderParams(TodParticle* theParticle, ParticleRenderParams* theParams);
	void							DrawParticle(Sexy::Graphics* g, TodParticle* theParticle, TodTriangleGroup* theTriangleGroup);
	void							UpdateSpawning();
	bool							UpdateParticle(TodParticle* theParticle);
	TodParticle*					SpawnParticle(int theIndex, int theSpawnCount);
	bool							CrossFadeParticle(TodParticle* theParticle, TodParticleEmitter* theToEmitter);
	void							CrossFadeEmitter(TodParticleEmitter* theToEmitter);
	bool							CrossFadeParticleToName(TodParticle* theParticle, const char* theEmitterName);
	void							DeleteAll();
	void							UpdateParticleField(TodParticle* theParticle, ParticleField* theParticleField, float theParticleTimeValue, int theFieldIndex);
	void							UpdateSystemField(ParticleField* theParticleField, float theParticleTimeValue, int theFieldIndex);
    float				            SystemTrackEvaluate(FloatParameterTrack& theTrack, ParticleSystemTracks theSystemTrack);
	static float		            ParticleTrackEvaluate(FloatParameterTrack& theTrack, TodParticle* theParticle, ParticleTracks theParticleTrack);
	void							DeleteParticle(TodParticle* theParticle);
	void							DeleteNonCrossFading();
};
float                               CrossFadeLerp(float theFrom, float theTo, bool theFromIsSet, bool theToIsSet, float theFraction);
void								RenderParticle(Sexy::Graphics* g, TodParticle* theParticle, const Sexy::Color& theColor, ParticleRenderParams* theParams, TodTriangleGroup* theTriangleGroup);

class TodParticleSystem
{
public:
	ParticleEffect					mEffectType;					//+0x0
    TodParticleDefinition*			mParticleDef;					//+0x4
    TodParticleHolder*				mParticleHolder;				//+0x8
    TodList<ParticleEmitterID>		mEmitterList;					//+0xC
    bool							mDead;							//+0x1C
    bool							mIsAttachment;					//+0x1D
    int								mRenderOrder;					//+0x20
    bool							mDontUpdate;					//+0x24

public:
    TodParticleSystem();
    ~TodParticleSystem();

    void							TodParticleInitializeFromDef(float theX, float theY, int theRenderOrder, TodParticleDefinition* theDefinition, ParticleEffect theEffectType);
    void							ParticleSystemDie();
    void							Update();
    void							Draw(Sexy::Graphics* g);
    void							SystemMove(float theX, float theY);
    void							OverrideColor(const char* theEmitterName, const Sexy::Color& theColor);
    void							OverrideExtraAdditiveDraw(const char* theEmitterName, bool theEnableExtraAdditiveDraw);
    void							OverrideImage(const char* theEmitterName, Sexy::Image* theImage);
    void							OverrideFrame(const char* theEmitterName, int theFrame);
    void							OverrideScale(const char* theEmitterName, float theScale);
    void							CrossFade(const char* theEmitterName);
    TodParticleEmitter*				FindEmitterByName(const char* theEmitterName);
    TodEmitterDefinition*			FindEmitterDefByName(const char* theEmitterName);
};

#endif
