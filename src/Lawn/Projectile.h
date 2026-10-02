#ifndef __PROJECTILE_H__
#define __PROJECTILE_H__

#include "GameObject.h"

class Plant;
class Zombie;
enum ZombieID;
enum AttachmentID;

enum ProjectileType
{
	PROJECTILE_PEA,			// 豌豆
	PROJECTILE_SNOWPEA,		// 冰豌豆
	PROJECTILE_CABBAGE,		// 卷心菜
	PROJECTILE_MELON,		// 西瓜
	PROJECTILE_PUFF,		// 孢子
	PROJECTILE_WINTERMELON, // 冰瓜
	PROJECTILE_FIREBALL,	// 火豌豆
	PROJECTILE_STAR,		// 星星
	PROJECTILE_SPIKE,		// 尖刺
	PROJECTILE_BASKETBALL,  // 篮球
	PROJECTILE_KERNEL,		// 玉米粒
	PROJECTILE_COBBIG,		// 玉米加农炮
	PROJECTILE_BUTTER,		// 黄油
	PROJECTILE_ZOMBIE_PEA,	// 僵尸豌豆
	PROJECTILE_CACTUS_SPEAR, // 【三十旗】精英僵尸「投矛」词条投出的仙人掌刺
	NUM_PROJECTILES
};

enum ProjectileMotion
{
	MOTION_STRAIGHT,		// 水平向右
	MOTION_LOBBED,			// 抛物线
	MOTION_THREEPEATER,		// 偏转向右
	MOTION_BEE,				// 
	MOTION_BEE_BACKWARDS,   // 
	MOTION_PUFF,			// 水平向右（一段时间后消失）
	MOTION_BACKWARDS,		// 水平向左
	MOTION_STAR,			// 斜向运动
	MOTION_FLOAT_OVER,		// 缓慢漂浮向右（无碰撞）
	MOTION_HOMING			// 追踪
};

class ProjectileDefinition
{
public:
	ProjectileType			mProjectileType;
	int						mImageRow;
	int						mDamage;
};
extern ProjectileDefinition gProjectileDefinition[NUM_PROJECTILES];  //0x69F1C0

class Projectile : public GameObject
{
public:
	int						mFrame;					//+0x24
	int						mNumFrames;				//+0x28
	int						mAnimCounter;			//+0x2C
	float					mPosX;					//+0x30
	float					mPosY;					//+0x34
	float					mPosZ;					//+0x38
	float					mVelX;					//+0x3C
	float					mVelY;					//+0x40
	float					mVelZ;					//+0x44
	float					mAccZ;					//+0x48
	float					mShadowY;				//+0x4C
	bool					mDead;					//+0x50
	int						mAnimTicksPerFrame;		//+0x54
	ProjectileMotion		mMotionType;			//+0x58
	ProjectileType			mProjectileType;		//+0x5C
	int						mProjectileAge;			//+0x60
	int						mClickBackoffCounter;   //+0x64
	float					mRotation;				//+0x68
	float					mRotationSpeed;			//+0x6C
	bool					mOnHighGround;			//+0x70
	int						mDamageRangeFlags;		//+0x74
	int						mHitTorchwoodGridX;		//+0x78
	AttachmentID			mAttachmentID;			//+0x7C
	float					mCobTargetX;			//+0x80
	int						mCobTargetRow;			//+0x84
	ZombieID				mTargetZombieID;		//+0x88
	int						mLastPortalX;			//+0x8C

	// 【三十旗】自定义字段。Projectile 存放在 DataArray<Projectile> 里，
	// Board 只持有该 DataArray 的指针，所以追加成员不会改变 sizeof(Board)，
	// 不影响 SyncBoard 的存档布局。
	int						mDamage;				// 附加伤害（随机多倍火球 / 逐株加成）
	int						mPierceLeft;			// 剩余穿透次数（豌豆连续命中）
	int						mElement;				// 【三十旗】元素火球类型（TF_ELEM_*，0=普通火球）

public:
	Projectile();
	~Projectile();

	void					ProjectileInitialize(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType);
	void					Update();
	void					Draw(Sexy::Graphics* g);
	void					DrawShadow(Sexy::Graphics* g);
	void					Die();
	void					DoImpact(Zombie* theZombie);
	void					UpdateMotion();
	void					CheckForCollision();
	Zombie*					FindCollisionTarget();
	void					UpdateLobMotion();
	void					CheckForHighGround();
	bool					CantHitHighGround();
	void					DoSplashDamage(Zombie* theZombie);
	ProjectileDefinition&   GetProjectileDef();
	unsigned int			GetDamageFlags(Zombie* theZombie);
	Sexy::Rect				GetProjectileRect();
	void					UpdateNormalMotion();
	Plant*					FindCollisionTargetPlant();
	void					ConvertToFireball(int theGridX);
	void					ConvertToPea(int theGridX);
	bool					IsSplashDamage(Zombie* theZombie);
	void					PlayImpactSound(Zombie* theZombie);
	bool					IsZombieHitBySplash(Zombie* theZombie);
	bool					PeaAboutToHitTorchwood();

};

#endif
