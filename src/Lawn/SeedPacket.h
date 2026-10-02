#ifndef __SEEDPACKET_H__
#define __SEEDPACKET_H__

#include "GameObject.h"

constexpr int   SEEDBANK_MAX			= 10;
constexpr int   SEED_BANK_OFFSET_X		= 0;
constexpr int   SEED_BANK_OFFSET_X_END  = 10;
constexpr int   SEED_CHOOSER_OFFSET_Y   = 516;
constexpr int   SEED_PACKET_WIDTH		= 50;
constexpr int   SEED_PACKET_HEIGHT		= 70;

class HitResult;

enum SeedType
{
	SEED_PEASHOOTER,
	SEED_SUNFLOWER,
	SEED_CHERRYBOMB,
	SEED_WALLNUT,
	SEED_POTATOMINE,
	SEED_SNOWPEA,
	SEED_CHOMPER,
	SEED_REPEATER,
	SEED_PUFFSHROOM,
	SEED_SUNSHROOM,
	SEED_FUMESHROOM,
	SEED_GRAVEBUSTER,
	SEED_HYPNOSHROOM,
	SEED_SCAREDYSHROOM,
	SEED_ICESHROOM,
	SEED_DOOMSHROOM,
	SEED_LILYPAD,
	SEED_SQUASH,
	SEED_THREEPEATER,
	SEED_TANGLEKELP,
	SEED_JALAPENO,
	SEED_SPIKEWEED,
	SEED_TORCHWOOD,
	SEED_TALLNUT,
	SEED_SEASHROOM,
	SEED_PLANTERN,
	SEED_CACTUS,
	SEED_BLOVER,
	SEED_SPLITPEA,
	SEED_STARFRUIT,
	SEED_PUMPKINSHELL,
	SEED_MAGNETSHROOM,
	SEED_CABBAGEPULT,
	SEED_FLOWERPOT,
	SEED_KERNELPULT,
	SEED_INSTANT_COFFEE,
	SEED_GARLIC,
	SEED_UMBRELLA,
	SEED_MARIGOLD,
	SEED_MELONPULT,
	SEED_GATLINGPEA,
	SEED_TWINSUNFLOWER,
	SEED_GLOOMSHROOM,
	SEED_CATTAIL,
	SEED_WINTERMELON,
	SEED_GOLD_MAGNET,
	SEED_SPIKEROCK,
	SEED_COBCANNON,
	SEED_IMITATER,
	SEED_EXPLODE_O_NUT,
	SEED_GIANT_WALLNUT,
	SEED_SPROUT,
	SEED_LEFTPEATER,
	NUM_SEED_TYPES,
	SEED_BEGHOULED_BUTTON_SHUFFLE,
	SEED_BEGHOULED_BUTTON_CRATER,
	SEED_SLOT_MACHINE_SUN,
	SEED_SLOT_MACHINE_DIAMOND,
	SEED_ZOMBIQUARIUM_SNORKEL,
	SEED_ZOMBIQUARIUM_TROPHY,
	SEED_ZOMBIE_NORMAL,
	SEED_ZOMBIE_TRAFFIC_CONE,
	SEED_ZOMBIE_POLEVAULTER,
	SEED_ZOMBIE_PAIL,
	SEED_ZOMBIE_LADDER,
	SEED_ZOMBIE_DIGGER,
	SEED_ZOMBIE_BUNGEE,
	SEED_ZOMBIE_FOOTBALL,
	SEED_ZOMBIE_BALLOON,
	SEED_ZOMBIE_SCREEN_DOOR,
	SEED_ZOMBONI,
	SEED_ZOMBIE_POGO,
	SEED_ZOMBIE_DANCER,
	SEED_ZOMBIE_GARGANTUAR,
	SEED_ZOMBIE_IMP,
	NUM_SEEDS_IN_CHOOSER = 49,
	SEED_NONE = -1
};

class SeedPacket : public GameObject
{
public:
	int					mRefreshCounter;							//+0x24
	int					mRefreshTime;								//+0x28
	int					mIndex;										//+0x2C
	int					mOffsetX;									//+0x30
	SeedType			mPacketType;								//+0x34
	SeedType			mImitaterType;								//+0x38
	int					mSlotMachineCountDown;						//+0x3C
	SeedType			mSlotMachiningNextSeed;						//+0x40
	float				mSlotMachiningPosition;						//+0x44
	bool				mActive;									//+0x48
	bool				mRefreshing;								//+0x49
	int					mTimesUsed;									//+0x4C

public:
	SeedPacket();

	void				Update();
	void				Draw(Sexy::Graphics* g);
	void				MouseDown(int x, int y, int theClickCount);
	bool				MouseHitTest(int theX, int theY, HitResult* theHitResult);
	void				Deactivate();
	void				Activate();
	void				SetActivate(bool theActivate);
	void				PickNextSlotMachineSeed();
	void				WasPlanted();
	void				SlotMachineStart();
	void				FlashIfReady();
	bool				CanPickUp();
	void				SetPacketType(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
};

class SeedBank : public GameObject
{
public:
	int					mNumPackets;					//+0x24
	SeedPacket			mSeedPackets[SEEDBANK_MAX];		//+0x28
	int					mCutSceneDarken;				//+0x348
	int					mConveyorBeltCounter;			//+0x34C

public:
	SeedBank();

	void				Draw(Sexy::Graphics* g);
	bool				MouseHitTest(int x, int y, HitResult* theHitResult);
	inline void			Move(int x, int y) { mX = x; mY = y; }
	bool				ContainsPoint(int theX, int theY);
	void				AddSeed(SeedType theSeedType, bool thePlaceOnLeft = false);
	void				RemoveSeed(int theIndex);
	int					GetNumSeedsOnConveyorBelt();
	int					CountOfTypeOnConveyorBelt(SeedType theSeedType);
	void				UpdateConveyorBelt();
	void				UpdateWidth();
	void				RefreshAllPackets();
};

void					SeedPacketDrawSeed(Sexy::Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float theOffsetX, float theOffsetY, float theScale);
void					DrawSeedPacket(Sexy::Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float thePercentDark, int theGrayness, bool theDrawCost, bool theUseCurrentCost);


#endif